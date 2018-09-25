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

// gdal
#include "cpl_http.h"

#include "catalog/file.h"
#include "catalog/folder.h"
#include "map/maptransform.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/stringutil.h"

namespace ngs {

//constexpr unsigned char LOCKS_EXTRA_COUNT = 10;

//------------------------------------------------------------------------------
// DownloadData
//------------------------------------------------------------------------------
constexpr unsigned short DOWNLOAD_THREAD_COUNT = 5;

class DownloadData : public ThreadData {
public:
    DownloadData(const std::string &basePath, const std::string &url, int expires,
                 const Tile &tile, const Options &options, bool own);
    Tile m_tile;
    std::string m_basePath, m_url;
    int m_expires;
    Options m_options;
};

DownloadData::DownloadData(const std::string &basePath, const std::string &url,
                           int expires, const Tile &tile, const Options &options,
                           bool own) :
    ThreadData(own),
    m_tile(tile),
    m_basePath(basePath),
    m_url(url),
    m_expires(expires),
    m_options(options)
{
}

//------------------------------------------------------------------------------
// Raster
//------------------------------------------------------------------------------

Raster::Raster(std::vector<std::string> siblingFiles,
               ObjectContainer * const parent,
               const enum ngsCatalogObjectType type,
               const std::string &name,
               const std::string &path) : Object(parent, type, name, path),
    DatasetBase(),
    SpatialDataset(),
    m_openFlags(GDAL_OF_SHARED|GDAL_OF_READONLY|GDAL_OF_VERBOSE_ERROR),
    m_siblingFiles(siblingFiles)
{
}

Raster::~Raster()
{
//    freeLocks(true);
    if(m_spatialReference)
        delete m_spatialReference;
}

bool Raster::open(unsigned int openFlags, const Options &options)
{
    if(isOpened()) {
        return true;
    }

    m_openFlags = openFlags;
    m_openOptions = options;

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

        int z_min = root.GetInteger(KEY_Z_MIN, 0);
        int z_max = root.GetInteger(KEY_Z_MAX, 18);
        bool y_origin_top = root.GetBool(KEY_Y_ORIGIN_TOP, true);
        unsigned short bandCount = static_cast<unsigned short>(
                    root.GetInteger(KEY_BAND_COUNT, 4));

        Envelope extent;
        extent.load(root.GetObj(KEY_EXTENT), DEFAULT_BOUNDS);
        m_extent.load(root.GetObj(KEY_LIMIT_EXTENT), DEFAULT_BOUNDS);
        int cacheExpires = root.GetInteger(KEY_CACHE_EXPIRES, defaultCacheExpires);
        int cacheMaxSize = root.GetInteger(KEY_CACHE_MAX_SIZE, defaultCacheMaxSize);
        const char *connStr = CPLSPrintf("<GDAL_WMS><Service name=\"TMS\">"
            "<ServerUrl>%s</ServerUrl></Service><DataWindow>"
            "<UpperLeftX>%f</UpperLeftX><UpperLeftY>%f</UpperLeftY>"
            "<LowerRightX>%f</LowerRightX><LowerRightY>%f</LowerRightY>"
            "<TileLevel>%d</TileLevel><TileCountX>1</TileCountX>"
            "<TileCountY>1</TileCountY><YOrigin>%s</YOrigin></DataWindow>"
            "<Projection>EPSG:%d</Projection><BlockSizeX>256</BlockSizeX>"
            "<BlockSizeY>256</BlockSizeY><BandsCount>%d</BandsCount>"
            "<Cache><Type>file</Type><Expires>%d</Expires><MaxSize>%d</MaxSize>"
            "</Cache><ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes></GDAL_WMS>",
                                         url.c_str(), extent.minX(),
                                         extent.maxY(), extent.maxX(),
                                         extent.minY(), z_max,
                                         y_origin_top ? "top" : "bottom",
                                         epsg, bandCount, cacheExpires,
                                         cacheMaxSize);

        bool result = DatasetBase::open(connStr, openFlags, options);
        if(result) {
            // Set NG_ADDITIONS metadata
            m_DS->SetMetadataItem("TMS_URL", url.c_str(), "");
            m_DS->SetMetadataItem("TMS_CACHE_EXPIRES", CPLSPrintf("%d", cacheExpires), "");
            m_DS->SetMetadataItem("TMS_CACHE_MAX_SIZE", CPLSPrintf("%d", cacheMaxSize), "");
            m_DS->SetMetadataItem("TMS_Y_ORIGIN_TOP", y_origin_top ? "top" : "bottom", "");
            m_DS->SetMetadataItem("TMS_Z_MIN", CPLSPrintf("%d", z_min), "");
            m_DS->SetMetadataItem("TMS_Z_MAX", CPLSPrintf("%d", z_max), "");
            m_DS->SetMetadataItem("TMS_X_MIN", CPLSPrintf("%f", extent.minX()), "");
            m_DS->SetMetadataItem("TMS_X_MAX", CPLSPrintf("%f", extent.maxX()), "");
            m_DS->SetMetadataItem("TMS_Y_MIN", CPLSPrintf("%f", extent.minY()), "");
            m_DS->SetMetadataItem("TMS_Y_MAX", CPLSPrintf("%f", extent.maxY()), "");

            m_DS->SetMetadataItem("TMS_LIMIT_X_MIN", CPLSPrintf("%f", m_extent.minX()), "");
            m_DS->SetMetadataItem("TMS_LIMIT_X_MAX", CPLSPrintf("%f", m_extent.maxX()), "");
            m_DS->SetMetadataItem("TMS_LIMIT_Y_MIN", CPLSPrintf("%f", m_extent.minY()), "");
            m_DS->SetMetadataItem("TMS_LIMIT_Y_MAX", CPLSPrintf("%f", m_extent.maxY()), "");

            // Set USER metadata
            CPLJSONObject user = root.GetObj(USER_KEY);
            if(user.IsValid()) {
                std::vector<CPLJSONObject> children = user.GetChildren();
                for(const CPLJSONObject &child : children) {
                    std::string name = child.GetName();
                    std::string value;
                    switch(child.GetType()) {
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
                        value = child.ToBool(true) ? "TRUE" : "FALSE";
                        break;
                    case CPLJSONObject::String:
                        value = child.ToString("");
                        break;
                    case CPLJSONObject::Integer:
                        value = std::to_string(child.ToInteger(0));
                        break;
                    case CPLJSONObject::Long:
                        value = std::to_string(child.ToLong(0));
                        break;
                    case CPLJSONObject::Double:
                        value = std::to_string(child.ToDouble(0.0));
                        break;
                    default:
                        value = "<unknown>";
                        break;
                    }
                    m_DS->SetMetadataItem(name.c_str(), value.c_str(), USER_KEY);
                }
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
                       bool skipLastBand/*, unsigned char zoom*/)
{
    if(!isOpened()) {
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

    MutexHolder holder(m_dataLock, 0.05);

    // Lock pixel area to read/write until exit
//    Mutex dataLock;

//    int deltaX = xSize - 1;
//    int deltaY = ySize - 1;
//    Envelope testEnv(xOff - deltaX, yOff - deltaY,
//                     xOff + xSize + deltaX, yOff + ySize + deltaY);
//    CPLAcquireMutex(m_dataLock, 15.0);

//    for(auto &lock : m_dataLocks) {
//        if(lock.env.intersects(testEnv) && lock.zoom == zoom) {
//            dataLock = lock.mutexRef;
//            break;
//        }
//    }
//    CPLReleaseMutex(m_dataLock);

//    bool exists = dataLock != nullptr;
//    if(!exists) {
//        MutexHolder holder(m_dataLock, 16.0);

//        dataLock = CPLCreateMutex();
//        m_dataLocks.push_back({testEnv, dataLock, zoom});
//    }

//    if(exists) {
//        CPLAcquireMutex(dataLock, 17.0);
//    }

    CPLErr result = m_DS->RasterIO(read ? GF_Read : GF_Write, xOff, yOff,
                                   xSize, ySize, data, bufXSize, bufYSize,
                                   dataType, skipLastBand ? bandCount - 1 :
                                                            bandCount, bandList,
                                   pixelSpace, lineSpace, bandSpace);

//    CPLReleaseMutex(dataLock);
//    freeLocks();

    if(result != CE_None) {
        return errorMessage(CPLGetLastErrorMsg());
    }

    return true;
}

bool Raster::destroy()
{
    if(Filter::isFileBased(m_type)) {
        if(File::deleteFile(m_path)) {
            Folder::rmDir(fromCString(m_DS->GetMetadataItem("CACHE_PATH")));
            std::string name = fullName();
            if(m_parent) {
                m_parent->notifyChanges();
            }
            Notify::instance().onNotify(name, ngsChangeCode::CC_DELETE_OBJECT);
            return  true;
        }
    }

    return errorMessage(_("The data type %d cannot be deleted. Path: %s"),
                        m_type, m_path.c_str());
}

bool Raster::canDestroy() const
{
    return Filter::isFileBased(m_type); // NOTE: Now supported only fila based rasters
}

Properties Raster::properties(const std::string &domain) const {
    if(nullptr == m_DS) {
        return Properties();
    }
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return Properties(m_DS->GetMetadata(domain.c_str()));
}

bool Raster::setProperty(const std::string &name, const std::string &value,
                         const std::string &domain)
{
    bool result = false;
    if(!isOpened()) {
        open();
    }
    if(m_DS != nullptr) {
        result = m_DS->SetMetadataItem(name.c_str(), value.c_str(), domain.c_str()) == CE_None;
    }

    if(result && compare(domain, USER_KEY)) {
        CPLJSONDocument connectionFile;
        if(!connectionFile.Load(m_path)) {
            return false;
        }
        CPLJSONObject root = connectionFile.GetRoot();

        bool needReopen = false;
        if(compare("TMS_CACHE_EXPIRES", name)) { // Only allow to change cache_expires
            root.Set(KEY_CACHE_EXPIRES, std::stoi(value));
            needReopen = true;
        }
        else if(compare("TMS_CACHE_MAX_SIZE", name)) { // Only allow to change cache_expires
            root.Set(KEY_CACHE_MAX_SIZE, std::stoi(value));
            needReopen = true;
        }
        else {
            CPLJSONObject user = root.GetObj(USER_KEY);
            if(!user.IsValid()) {
                user.Set(name, value);
            }
            else {
                CPLJSONObject newUser;
                newUser.Add(name, value);
                root.Add(USER_KEY, newUser);
            }
        }

        result = connectionFile.Save(m_path);        
        if(!result) {
            return result;
        }

        if(needReopen && m_type == CAT_RASTER_TMS) {
            close();
            return open(m_openFlags, m_openOptions);
        }
    }
    return result;
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

/*
void Raster::freeLocks(bool all)
{
    unsigned char threadCount = static_cast<unsigned char>(
                atoi(CPLGetConfigOption("GDAL_NUM_THREADS", "1")));
    MutexHolder holder(m_dataLock, 50.0);
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
*/

std::string Raster::options(ngsOptionType optionType) const
{
    return DatasetBase::options(m_type, optionType);
}

bool Raster::writeWorldFile(enum WorldFileType type)
{
    std::string ext = File::getExtension(m_path);
    std::string newExt;

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
    return GDALWriteWorldFile(m_path.c_str(), newExt.c_str(),
                              geoTransform) == 0 ? false : true;
}

bool Raster::geoTransform(double *transform) const
{
    if(!isOpened()) {
        return false;
    }
    return m_DS->GetGeoTransform(transform) == CE_None;
}

int Raster::width() const
{
    if(!isOpened()) {
        return 0;
    }
    return m_DS->GetRasterXSize();
}

int Raster::height() const
{
    if(!isOpened()) {
        return 0;
    }
    return m_DS->GetRasterYSize();
}

int Raster::dataSize() const
{
    if(!isOpened() || m_DS->GetRasterCount() == 0) {
        return 0;
    }
    GDALDataType dt = m_DS->GetRasterBand(1)->GetRasterDataType();
    return GDALGetDataTypeSize(dt) / 8;
}

unsigned short Raster::bandCount() const
{
    if(!isOpened()) {
        return 0;
    }
    return static_cast<unsigned short>(m_DS->GetRasterCount());
}

GDALDataType Raster::dataType(int band) const
{
    if(!isOpened() || m_DS->GetRasterCount() == 0) {
        return GDT_Unknown;
    }
    return  m_DS->GetRasterBand(band)->GetRasterDataType();
}

int Raster::getBestOverview(int &xOff, int &yOff, int &xSize, int &ySize,
                            int bufXSize, int bufYSize) const
{
    if(!isOpened() || m_DS->GetRasterCount() == 0) {
        return 0;
    }
    return GDALBandGetBestOverviewLevel2(m_DS->GetRasterBand(1), xOff, yOff,
                                         xSize, ySize, bufXSize, bufYSize, nullptr);
}

static CPLLock *hLock = nullptr;

bool Raster::cacheAreaJobThreadFunc(ThreadData* threadData)
{
    DownloadData* data = dynamic_cast<DownloadData*>(threadData);
    if(nullptr == data) {
        return true;
    }

    CPLString url(data->m_url);
    url = url.replaceAll("${x}", std::to_string(data->m_tile.x));
    url = url.replaceAll("${y}", std::to_string(data->m_tile.y));
    url = url.replaceAll("${z}", std::to_string(data->m_tile.z));

    std::string fileName = md5(url);
    std::string dirPath = CPLSPrintf("%c/%c", fileName[0], fileName[1]);
    std::string path = File::formFileName(data->m_basePath, dirPath, "");

    if(!Folder::isExists(path)) {
        CPLLockHolderD(&hLock, LOCK_RECURSIVE_MUTEX);

        if(!Folder::mkDir(path, true)) {
            return false;
        }
    }

    path = File::formFileName(path, fileName, "");
    File::modificationDate(path);
    if(time(nullptr) - File::modificationDate(path) < data->m_expires) {
        return true;
    }

    // Download tile and save it to cache
    auto optionsPtr = data->m_options.asCharArray();
    CPLHTTPResult *result = CPLHTTPFetch(url, optionsPtr.get());
    if(result->nStatus != 0) {
        outMessage(COD_REQUEST_FAILED, result->pszErrBuf);
        CPLHTTPDestroyResult(result);
        return false;
    }

    bool out = File::writeFile(path, result->pabyData,
                               static_cast<size_t>(result->nDataLen));

    CPLHTTPDestroyResult(result);

    return out;
}

bool Raster::cacheArea(const Progress &progress, const Options &options)
{
    if(!isOpened()) {
        return outMessage(COD_UNSUPPORTED,
                          "Raster must be oppened.");
    }
    if(m_type != CAT_RASTER_TMS) {
        return outMessage(COD_UNSUPPORTED,
                          "Unsupported type of raster. Mast be web based like TMS, WMS, etc.");
    }

    double minX = options.asDouble("MINX", DEFAULT_BOUNDS.minX());
    double minY = options.asDouble("MINY", DEFAULT_BOUNDS.minY());
    double maxX = options.asDouble("MAXX", DEFAULT_BOUNDS.maxX());
    double maxY = options.asDouble("MAXY", DEFAULT_BOUNDS.maxY());
    Envelope extent(minX, minY, maxX, maxY);

    std::set<unsigned char> zoomLevels;

    const std::string zoomLevelListStr = options.asString("ZOOM_LEVELS", "");
    char **zoomLevelArray = CSLTokenizeString2(zoomLevelListStr.c_str(), ",", 0);
    if(nullptr != zoomLevelArray) {
        int i = 0;
        const char *zoomLevel;
        while((zoomLevel = zoomLevelArray[i++]) != nullptr) {
            zoomLevels.insert(static_cast<unsigned char>(std::stoi(zoomLevel)));
        }
        CSLDestroy(zoomLevelArray);
    }

    if(zoomLevels.empty()) {
        return outMessage(COD_UNSUPPORTED, _("Zoom level list is empty."));
    }

    Options loadOptions(options);
    loadOptions.remove("MINX");
    loadOptions.remove("MINY");
    loadOptions.remove("MAXX");
    loadOptions.remove("MAXY");
    loadOptions.remove("ZOOM_LEVELS");

    // Get cache path
    std::string basePath = fromCString(m_DS->GetMetadataItem("CACHE_PATH"));
    std::string url = fromCString(m_DS->GetMetadataItem("TMS_URL"));
    const char *strExpires = m_DS->GetMetadataItem("TMS_CACHE_EXPIRES");
    int expires = std::stoi(strExpires == nullptr ? "0" : strExpires);

    bool reverseY = false;
    if(compare(fromCString(m_DS->GetMetadataItem("TMS_Y_ORIGIN_TOP")), "top")) {
        reverseY = true;
    }

    // Get tiles list
    progress.onProgress(COD_IN_PROCESS, 0.0, _("Start download area..."));

    // Multithreaded thread pool
    CPLDebug("ngstore", "cache area");

    ThreadPool threadPool;
    threadPool.init(DOWNLOAD_THREAD_COUNT, cacheAreaJobThreadFunc, 3, true);

    for(auto zoomLevel : zoomLevels) {
        std::vector<TileItem> items =
                MapTransform::getTilesForExtent(extent, zoomLevel, reverseY,
                                                true);

        for(auto item : items) {
            threadPool.addThreadData(new DownloadData(basePath, url, expires,
                                                      item.tile, loadOptions,
                                                      true));
        }
    }

    threadPool.waitComplete(progress);
    threadPool.clearThreadData();

    if(threadPool.isFailed()) {
        progress.onProgress(COD_GET_FAILED, 1.0, _("Download area failed"));
        return false;
    }
    progress.onProgress(COD_FINISHED, 1.0, _("Finish download area"));

    CPLDebug("ngstore", "finish cache area");
    return true;
}


} // namespace ngs
