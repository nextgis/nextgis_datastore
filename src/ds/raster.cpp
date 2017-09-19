/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "raster.h"

#include "catalog/file.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"
#include "util/notify.h"

namespace ngs {

constexpr unsigned char LOCKS_EXTRA_COUNT = 10;

Raster::Raster(std::vector<CPLString> siblingFiles,
               ObjectContainer * const parent,
               const enum ngsCatalogObjectType type,
               const CPLString &name,
               const CPLString &path) :
  Object(parent, type, name, path),
  DatasetBase(),
  SpatialDataset(),
  m_siblingFiles(siblingFiles),
  m_dataLock(CPLCreateMutex())
{
    CPLReleaseMutex(m_dataLock);
}

Raster::~Raster()
{
    freeLocks(true);
    CPLDestroyMutex(m_dataLock);
    if(m_spatialReference)
        delete m_spatialReference;
}

bool Raster::open(unsigned int openFlags, const Options &options)
{
    if(isOpened())
        return true;

    if(m_type == CAT_RASTER_TMS) {
        CPLJSONDocument connectionFile;
        if(!connectionFile.Load(m_path)) {
            return false;
        }
        CPLJSONObject root = connectionFile.GetRoot();
        CPLString url = root.GetString(KEY_URL, "");
        url = url.replaceAll("{", "${");
        url = url.replaceAll("&", "&amp;");
        int epsg = root.GetInteger(KEY_EPSG, 3857);

        m_spatialReference = new OGRSpatialReference;
        m_spatialReference->importFromEPSG(epsg);

        // TODO: Add support for "cache_expires" value and "user" JSONObject;
//        int z_min = root.getInteger(KEY_Z_MIN, 0);
        int z_max = root.GetInteger(KEY_Z_MAX, 18);
        bool y_origin_top = root.GetBool(KEY_Y_ORIGIN_TOP, true);
        unsigned short bandCount = static_cast<unsigned short>(
                    root.GetInteger(KEY_BAND_COUNT, 4));

        Envelope extent;
        extent.load(root.GetObject(KEY_EXTENT), DEFAULT_BOUNDS);
        m_extent.load(root.GetObject(KEY_LIMIT_EXTENT), DEFAULT_BOUNDS);

        const char* connStr = CPLSPrintf("<GDAL_WMS><Service name=\"TMS\">"
            "<ServerUrl>%s</ServerUrl></Service><DataWindow>"
            "<UpperLeftX>%f</UpperLeftX><UpperLeftY>%f</UpperLeftY>"
            "<LowerRightX>%f</LowerRightX><LowerRightY>%f</LowerRightY>"
            "<TileLevel>%d</TileLevel><TileCountX>1</TileCountX>"
            "<TileCountY>1</TileCountY><YOrigin>%s</YOrigin></DataWindow>"
            "<Projection>EPSG:%d</Projection><BlockSizeX>256</BlockSizeX>"
            "<BlockSizeY>256</BlockSizeY><BandsCount>%d</BandsCount>"
            "<Cache/></GDAL_WMS>",
                                         url.c_str(), extent.minX(),
                                         extent.maxY(), extent.maxX(),
                                         extent.minY(), z_max,
                                         y_origin_top ? "top" : "bottom",
                                         epsg, bandCount);

        bool result = DatasetBase::open(connStr, openFlags, options);
        if(result) {
            CPLJSONObject user = root.GetObject(KEY_USER);
            if(user.IsValid()) {
                CPLJSONObject **children = user.GetChildren();
                int i = 0;
                CPLJSONObject *child = nullptr;
                while((child = children[i++]) != nullptr) {
                    const char *name = child->GetName();
                    CPLString value;
                    switch(child->GetType()) {
                    case CPLJSONObject::Null:
                        value = "<null>";
                        break;
                    case CPLJSONObject::Object:
                        value = "<object>";
                        break;
                    case CPLJSONObject::Array:
                        value = "<array>";
                        break;
                    case CPLJSONObject::Boolean:
                        value = child->GetBool(true) ? "TRUE" : "FALSE";
                        break;
                    case CPLJSONObject::String:
                        value = child->GetString("");
                        break;
                    case CPLJSONObject::Integer:
                        value = CPLSPrintf("%d", child->GetInteger(0));
                        break;
                    case CPLJSONObject::Long:
                        value = CPLSPrintf("%ld", child->GetLong(0));
                        break;
                    case CPLJSONObject::Double:
                        value = CPLSPrintf("%f", child->GetDouble(0.0));
                        break;
                    }
                    m_DS->SetMetadataItem(name, value, KEY_USER);
                }
                CPLJSONObject::DestroyJSONObjectList(children);
            }
        }
        return result;
    }
    else {
        if(DatasetBase::open(m_path, openFlags, options)) {
            const char *spatRefStr = m_DS->GetProjectionRef();
            if(spatRefStr != nullptr) {
                m_spatialReference = new OGRSpatialReference;
                m_spatialReference->SetFromUserInput(spatRefStr);
            }
            setExtent();
            return true;
        }
    }
    return false;
}

bool Raster::pixelData(void *data, int xOff, int yOff, int xSize, int ySize,
                       int bufXSize, int bufYSize, GDALDataType dataType,
                       int bandCount, int *bandList, bool read,
                       bool skipLastBand, unsigned char zoom)
{
    //CPLMutexHolder holder(m_dataLock);
    if(nullptr == m_DS) {
        return false;
    }

    CPLErrorReset();
    int pixelSpace(0);
    int lineSpace(0);
    int bandSpace(0);
    if(bandCount > 1) {
        int dataSize = GDALGetDataTypeSize(dataType) / 8;
        pixelSpace = dataSize * bandCount;
        lineSpace = bufXSize * pixelSpace;
        bandSpace = dataSize;
    }

    // Lock pixel area to read/write until exit
    CPLMutex *dataLock = nullptr;

    Envelope testEnv(xOff - (xSize * 0.5), yOff - (ySize * 0.5),
                     xOff + xSize + xSize * 0.5, yOff + ySize + ySize * 0.5);
    CPLAcquireMutex(m_dataLock, 15.0);

    for(auto &lock : m_dataLocks) {
        if(lock.env.intersects(testEnv) && lock.zoom == zoom) {
            dataLock = lock.mutexRef;
            break;
        }
    }
    CPLReleaseMutex(m_dataLock);

    bool exists = dataLock != nullptr;
    if(!exists) {
        CPLMutexHolder holder(m_dataLock, 16.0);

        dataLock = CPLCreateMutex();
        m_dataLocks.push_back({testEnv, dataLock, zoom});
    }

    if(exists) {
        CPLAcquireMutex(dataLock, 17.0);
    }

    CPLErr result = m_DS->RasterIO(read ? GF_Read : GF_Write, xOff, yOff,
                                   xSize, ySize, data, bufXSize, bufYSize,
                                   dataType, skipLastBand ? bandCount - 1 :
                                                            bandCount, bandList,
                                   pixelSpace, lineSpace, bandSpace);

    CPLReleaseMutex(dataLock);
    freeLocks();

    if(result != CE_None) {
        return errorMessage(CPLGetLastErrorMsg());
    }

    return true;
}

bool Raster::destroy()
{
    if(Filter::isFileBased(m_type)) {
        if(File::deleteFile(m_path)) {
            CPLString name = fullName();
            if(m_parent)
                m_parent->notifyChanges();
            Notify::instance().onNotify(name, ngsChangeCode::CC_DELETE_OBJECT);
            return  true;
        }
    }

    errorMessage(COD_DELETE_FAILED, _("The data type %d cannot be deleted. Path: %s"),
                 m_type, m_path.c_str());

    return false;
}

bool Raster::canDestroy() const
{
    return Filter::isFileBased(m_type); // NOTE: Now supported only fila based rasters
}

void Raster::setExtent()
{
    double geoTransform[6] = {0};
    int xSize = m_DS->GetRasterXSize();
    int ySize = m_DS->GetRasterYSize();
    if(m_DS->GetGeoTransform(geoTransform) == CE_None) {
        double inX[4];
        double inY[4];

        inX[0] = 0;
        inY[0] = 0;
        inX[1] = xSize;
        inY[1] = 0;
        inX[2] = xSize;
        inY[2] = ySize;
        inX[3] = 0;
        inY[3] = ySize;

        m_extent.setMaxX(-1000000000.0);
        m_extent.setMaxY(-1000000000.0);
        m_extent.setMinX(1000000000.0);
        m_extent.setMinY(1000000000.0);

        for(int i = 0; i < 4; ++i) {
            double rX, rY;
            GDALApplyGeoTransform( geoTransform, inX[i], inY[i], &rX, &rY );
            if(m_extent.maxX() < rX) {
                m_extent.setMaxX(rX);
            }
            if(m_extent.minX() > rX) {
                m_extent.setMinX(rX);
            }
            if(m_extent.maxY() < rY) {
                m_extent.setMaxY(rY);
            }
            if(m_extent.minY() > rY) {
                m_extent.setMinY(rY);
            }
        }
    }
    else {
        m_extent.setMaxX(xSize);
        m_extent.setMaxY(ySize);
        m_extent.setMinX(0);
        m_extent.setMinY(0);
    }
}

void Raster::freeLocks(bool all)
{
    unsigned char threadCount = static_cast<unsigned char>(
                atoi(CPLGetConfigOption("GDAL_NUM_THREADS", "1")));
    CPLMutexHolder holder(m_dataLock, 50.0);
    if(all) {
        for(auto &lock : m_dataLocks) {
            CPLAcquireMutex(lock.mutexRef, 1000.0);
            CPLReleaseMutex(lock.mutexRef);
            CPLDestroyMutex(lock.mutexRef);
        }
        m_dataLocks.clear();
    }
    else {
        if(m_dataLocks.size() > threadCount * LOCKS_EXTRA_COUNT) {
            size_t freeCount = m_dataLocks.size() - threadCount;
            for(size_t i = 0; i < freeCount; ++i) {
                if(m_dataLocks[i].mutexRef) {
                    CPLAcquireMutex(m_dataLocks[i].mutexRef, 5.0);
                    CPLReleaseMutex(m_dataLocks[i].mutexRef);
                    CPLDestroyMutex(m_dataLocks[i].mutexRef);
                    m_dataLocks[i].mutexRef = nullptr;
                }
                else {
                    m_dataLocks.erase(m_dataLocks.begin(), m_dataLocks.begin() +
                                        i);
                    return;
                }
            }
            m_dataLocks.erase(m_dataLocks.begin(), m_dataLocks.begin() +
                                freeCount);
        }
    }
}

const char *Raster::options(ngsOptionType optionType) const
{
    return DatasetBase::options(m_type, optionType);
}

bool Raster::writeWorldFile(enum WorldFileType type)
{
    CPLString ext = CPLGetExtension(m_path);
    CPLString newExt;

    switch(type) {
    case FIRSTLASTW:
        newExt += ext.front();
        newExt += ext.back();
        newExt += 'w';
        break;
    case EXTPLUSWX:
        newExt += ext.front();
        newExt += ext.back();
        newExt += 'w';
        newExt += 'x';
        break;
    case WLD:
        newExt.append("wld");
        break;
    case EXTPLUSW:
        newExt = ext + CPLString("w");
        break;
    }

    double geoTransform[6] = { 0 };
    if(m_DS->GetGeoTransform(geoTransform) != CE_None) {
        return errorMessage(CPLGetLastErrorMsg());
    }
    return GDALWriteWorldFile(m_path, newExt, geoTransform) == 0 ? false : true;
}

bool Raster::geoTransform(double *transform) const
{
    if(!isOpened())
        return false;
    return m_DS->GetGeoTransform(transform) == CE_None;
}

int Raster::width() const
{
    if(!isOpened())
        return 0;
    return m_DS->GetRasterXSize();
}

int Raster::height() const
{
    if(!isOpened())
        return 0;
    return m_DS->GetRasterYSize();
}

int Raster::dataSize() const
{
    if(!isOpened())
        return 0;
    if(m_DS->GetRasterCount() == 0)
        return 0;
    GDALDataType dt = m_DS->GetRasterBand(1)->GetRasterDataType();
    return GDALGetDataTypeSize(dt) / 8;
}

unsigned short Raster::bandCount() const
{
    if(!isOpened())
        return 0;
    return static_cast<unsigned short>(m_DS->GetRasterCount());
}

GDALDataType Raster::dataType(int band) const
{
    if(!isOpened())
        return GDT_Unknown;
    if(m_DS->GetRasterCount() == 0)
        return GDT_Unknown;
    return  m_DS->GetRasterBand(band)->GetRasterDataType();
}

int Raster::getBestOverview(int &xOff, int &yOff, int &xSize, int &ySize,
                            int bufXSize, int bufYSize) const
{
    if(!isOpened())
        return 0;
    if(m_DS->GetRasterCount() == 0)
        return 0;

    return GDALBandGetBestOverviewLevel2(m_DS->GetRasterBand(1), xOff, yOff,
                                         xSize, ySize, bufXSize, bufYSize, nullptr);
}

} // namespace ngs
