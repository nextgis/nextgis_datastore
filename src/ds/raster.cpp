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

#include "util/error.h"

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
        JSONDocument connectionFile;
        if(!connectionFile.load(m_path)) {
            return false;
        }
        JSONObject root = connectionFile.getRoot();
        CPLString url = root.getString(KEY_URL, "");
        url = url.replaceAll("{", "${");
        int epsg = root.getInteger(KEY_EPSG, 3857);

        m_spatialReference = new OGRSpatialReference;
        m_spatialReference->importFromEPSG(epsg);

//        int z_min = root.getInteger(KEY_Z_MIN, 0);
        int z_max = root.getInteger(KEY_Z_MAX, 18);
        bool y_origin_top = root.getBool(KEY_Y_ORIGIN_TOP, true);

        m_extent.load(root.getObject(KEY_EXTENT), DEFAULT_BOUNDS);

        const char* connStr = CPLSPrintf("<GDAL_WMS><Service name=\"TMS\">"
            "<ServerUrl>%s</ServerUrl></Service><DataWindow>"
            "<UpperLeftX>%f</UpperLeftX><UpperLeftY>%f</UpperLeftY>"
            "<LowerRightX>%f</LowerRightX><LowerRightY>%f</LowerRightY>"
            "<TileLevel>%d</TileLevel><TileCountX>1</TileCountX>"
            "<TileCountY>1</TileCountY><YOrigin>%s</YOrigin></DataWindow>"
            "<Projection>EPSG:%d</Projection><BlockSizeX>256</BlockSizeX>"
            "<BlockSizeY>256</BlockSizeY><BandsCount>3</BandsCount>"
            "<Cache/></GDAL_WMS>",
                                         url.c_str(), m_extent.getMinX(),
                                         m_extent.getMaxY(), m_extent.getMaxX(),
                                         m_extent.getMinY(), z_max,
                                         y_origin_top ? "top" : "bottom",
                                         epsg);

        return DatasetBase::open(connStr, openFlags, options);
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
    CPLErrorReset();
    int pixelSpace(0);
    int lineSpace(0);
    int bandSpace(0);
    if(bandCount > 1)
    {
        int dataSize = GDALGetDataTypeSize(dataType) / 8;
        pixelSpace = dataSize * bandCount;
        lineSpace = bufXSize * pixelSpace;
        bandSpace = dataSize;
    }

    // Lock pixel area to read/write until exit
    std::timed_mutex* dataLock = nullptr;

    Envelope testEnv(xOff, yOff, xOff + xSize, yOff + ySize);
    CPLAcquireMutex(m_dataLock, 50.0);

    for(auto &lock : m_dataLocks) {
        if(lock.env.intersects(testEnv) && lock.zoom == zoom) {
            dataLock = lock.mutexRef;
            break;
        }
    }
    CPLReleaseMutex(m_dataLock);

    if(!dataLock) {
        CPLMutexHolder holder(m_dataLock, 50.0);
        dataLock = new std::timed_mutex;
        m_dataLocks.push_back({testEnv, dataLock, zoom});
    }
    using sec = std::chrono::seconds;
    bool success = dataLock->try_lock_for(sec(60));
    if(!success) {
        return false;
    }

    CPLErr result = m_DS->RasterIO(read ? GF_Read : GF_Write, xOff, yOff,
                                   xSize, ySize, data, bufXSize, bufYSize,
                                   dataType, skipLastBand ? bandCount - 1 :
                                                            bandCount, bandList,
                                   pixelSpace, lineSpace, bandSpace);

    dataLock->unlock();
    freeLocks();

    if(result != CE_None) {
        return errorMessage(CPLGetLastErrorMsg());
    }

    return true;
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
            if(m_extent.getMaxX() < rX) {
                m_extent.setMaxX(rX);
            }
            if(m_extent.getMinX() > rX) {
                m_extent.setMinX(rX);
            }
            if(m_extent.getMaxY() < rY) {
                m_extent.setMaxY(rY);
            }
            if(m_extent.getMinY() > rY) {
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
            if(lock.mutexRef->try_lock()) {
                lock.mutexRef->unlock();
                delete lock.mutexRef;
            }
        }
        m_dataLocks.clear();
    }
    else {
        if(m_dataLocks.size() > threadCount * LOCKS_EXTRA_COUNT) {
            size_t freeCount = m_dataLocks.size() - threadCount;
            for(size_t i = 0; i < freeCount; ++i) {
                if(m_dataLocks[i].mutexRef) {
                    if(m_dataLocks[i].mutexRef->try_lock()) {
                        m_dataLocks[i].mutexRef->unlock();
                        delete m_dataLocks[i].mutexRef;
                        m_dataLocks[i].mutexRef = nullptr;
                    }
                    else {
                        m_dataLocks.erase(m_dataLocks.begin(), m_dataLocks.begin() +
                                            i);
                        return;
                    }
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

bool Raster::getGeoTransform(double *transform) const
{
    if(!isOpened())
        return false;
    return m_DS->GetGeoTransform(transform) == CE_None;
}

int Raster::getWidth() const
{
    if(!isOpened())
        return 0;
    return m_DS->GetRasterXSize();
}

int Raster::getHeight() const
{
    if(!isOpened())
        return 0;
    return m_DS->GetRasterYSize();
}

int Raster::getDataSize() const
{
    if(!isOpened())
        return 0;
    if(m_DS->GetRasterCount() == 0)
        return 0;
    GDALDataType dt = m_DS->GetRasterBand(1)->GetRasterDataType();
    return GDALGetDataTypeSize(dt) / 8;
}

GDALDataType Raster::getDataType(int band) const
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
