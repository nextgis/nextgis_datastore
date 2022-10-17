/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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
#include "util/settings.h"
#include "util/stringutil.h"

namespace ngs {

//------------------------------------------------------------------------------
// DownloadData
//------------------------------------------------------------------------------
constexpr unsigned short DOWNLOAD_THREAD_COUNT = 5;

class DownloadData : public ThreadData {
public:
    DownloadData(const std::string &basePath, const std::string &url, int expires,
                 const Tile &tile, const Options &options, bool own);
    virtual ~DownloadData() override;
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

DownloadData::~DownloadData()
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
        CPLString url = root.GetString(URL_KEY, "");
        url = url.replaceAll("{", "${");
        url = url.replaceAll("&", "&amp;");
        int epsg = root.GetInteger(KEY_EPSG, DEFAULT_EPSG);

        m_spatialReference = SpatialReferencePtr::importFromEPSG(epsg);

        int z_min = root.GetInteger(KEY_Z_MIN, 0);
        int z_max = root.GetInteger(KEY_Z_MAX, DEFAULT_MAX_ZOOM);
        bool y_origin_top = root.GetBool(KEY_Y_ORIGIN_TOP, true);
        unsigned short bandCount = static_cast<unsigned short>(
                    root.GetInteger(KEY_BAND_COUNT, 4));

        Envelope extent;
        extent.load(root.GetObj(KEY_EXTENT), DEFAULT_BOUNDS);
        m_extent.load(root.GetObj(KEY_LIMIT_EXTENT), DEFAULT_BOUNDS);
        int cacheExpires = root.GetInteger(KEY_CACHE_EXPIRES, defaultCacheExpires);
        int cacheMaxSize = root.GetInteger(KEY_CACHE_MAX_SIZE, defaultCacheMaxSize);

        const Settings &settings = Settings::instance();
        int timeout = settings.getInteger("http/timeout", 5);

        const char *connStr = CPLSPrintf("<GDAL_WMS><Service name=\"TMS\">"
            "<ServerUrl>%s</ServerUrl></Service><DataWindow>"
            "<UpperLeftX>%f</UpperLeftX><UpperLeftY>%f</UpperLeftY>"
            "<LowerRightX>%f</LowerRightX><LowerRightY>%f</LowerRightY>"
            "<TileLevel>%d</TileLevel><TileCountX>1</TileCountX>"
            "<TileCountY>1</TileCountY><YOrigin>%s</YOrigin></DataWindow>"
            "<Projection>EPSG:%d</Projection><BlockSizeX>256</BlockSizeX>"
            "<BlockSizeY>256</BlockSizeY><BandsCount>%d</BandsCount>"
            "<Cache><Type>file</Type><Expires>%d</Expires><MaxSize>%d</MaxSize>"
            "</Cache><MaxConnections>1</MaxConnections><Timeout>%d</Timeout><AdviseRead>false</AdviseRead>"
            "<ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes></GDAL_WMS>",
            url.c_str(), extent.minX(), extent.maxY(), extent.maxX(), extent.minY(), z_max,
            y_origin_top ? "top" : "bottom", epsg, bandCount, cacheExpires, cacheMaxSize, timeout);

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
                    case CPLJSONObject::Type::Null:
                        value = "<null>";
                        break;
                    case CPLJSONObject::Type::Object:
                        value = "<object>";
                        break;
                    case CPLJSONObject::Type::Array:
                        value = "<array>";
                        break;
                    case CPLJSONObject::Type::Boolean:
                        value = child.ToBool(true) ? "TRUE" : "FALSE";
                        break;
                    case CPLJSONObject::Type::String:
                        value = child.ToString("");
                        break;
                    case CPLJSONObject::Type::Integer:
                        value = std::to_string(child.ToInteger(0));
                        break;
                    case CPLJSONObject::Type::Long:
                        value = std::to_string(child.ToLong(0));
                        break;
                    case CPLJSONObject::Type::Double:
                        value = std::to_string(child.ToDouble(0.0));
                        break;
                    default:
                        value = "<unknown>";
                        break;
                    }
                    m_DS->SetMetadataItem(name.c_str(), value.c_str(), USER_KEY);
                }
            }

            // Set pixel limits from m_extent
            double geoTransform[6] = { 0.0 };
            double invGeoTransform[6] = { 0.0 };
            bool noTransform = false;
            if(m_DS->GetGeoTransform(geoTransform) == CE_None) {
                noTransform = GDALInvGeoTransform(geoTransform, invGeoTransform) == 0;
            }

            if(noTransform) {
                double minX, minY, maxX, maxY;
                GDALApplyGeoTransform(invGeoTransform, m_extent.minX(),
                                      m_extent.minY(), &minX, &maxY);

                GDALApplyGeoTransform(invGeoTransform, m_extent.maxX(),
                                      m_extent.maxY(), &maxX, &minY);
                m_pixelExtent = Envelope(minX, minY, maxX, maxY);
            }
            else {
                m_pixelExtent = Envelope(0.0, 0.0,
                    std::numeric_limits<double>::max(),
                    std::numeric_limits<double>::max());
            }
        }
        return result;
    }
    else {
        if(DatasetBase::open(m_path, openFlags, options)) {
            std::string spatRefStr = m_DS->GetProjectionRef();
            m_spatialReference.setFromUserInput(spatRefStr);
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
        int dataSize = GDALGetDataTypeSizeBytes(dataType);
        pixelSpace = dataSize * bandCount;
        lineSpace = bufXSize * pixelSpace;
        bandSpace = dataSize;
    }

    // Check if m_extent intersects pixel bounds
    // return false to get null tile
    if(!m_pixelExtent.intersects(
        Envelope(static_cast<double>(xOff), static_cast<double>(yOff),
                 static_cast<double>(xOff + xSize),
                 static_cast<double>(yOff + ySize)))) {
        return false;
    }

    MutexHolder holder(m_dataLock, 0.05);

    CPLErr result = m_DS->RasterIO(read ? GF_Read : GF_Write, xOff, yOff,
                                   xSize, ySize, data, bufXSize, bufYSize,
                                   dataType, skipLastBand ? bandCount - 1 :
                                                            bandCount, bandList,
                                   pixelSpace, lineSpace, bandSpace);

    if(result != CE_None) {
        return errorMessage(CPLGetLastErrorMsg());
    }

    return true;
}

bool Raster::destroy()
{
    if(Filter::isFileBased(m_type)) {
        if(File::deleteFile(m_path)) {
            if(Folder::rmDir(fromCString(m_DS->GetMetadataItem("CACHE_PATH")))) {
                return Object::destroy();
            }
        }
    }

    return errorMessage(_("The data type %d cannot be deleted. Path: %s"),
                        m_type, m_path.c_str());
}

bool Raster::canDestroy() const
{
    return Filter::isFileBased(m_type); // NOTE: Now supported only file based raster sources
}

Properties Raster::properties(const std::string &domain) const {
    if(nullptr == m_DS) {
        return Object::properties(domain);
    }
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return Properties(m_DS->GetMetadata(domain.c_str()));
}

std::string Raster::property(const std::string &key,
                     const std::string &defaultValue,
                     const std::string &domain) const
{
    if(nullptr == m_DS) {
        return Object::property(key, defaultValue, domain);
    }
    auto ret = m_DS->GetMetadataItem(key.c_str(), domain.c_str());
    if(nullptr == ret) {
        return Object::property(key, defaultValue, domain);
    }
    return ret;
}

bool Raster::setProperty(const std::string &name, const std::string &value,
                         const std::string &domain)
{
    bool result = false;
    if(!isOpened()) {
        open();
    }
    if(m_DS != nullptr) {
        result = m_DS->SetMetadataItem(name.c_str(), value.c_str(),
                                       domain.c_str()) == CE_None;
    }

    if(result) {
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
        else if(compare("TMS_CACHE_MAX_SIZE", name)) { // Only allow to change cache_max_size
            root.Set(KEY_CACHE_MAX_SIZE, std::stoi(value));
            needReopen = true;
        }
        else if(compare(domain, USER_KEY)){
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
    double geoTransform[6] = {0.0};
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
    m_pixelExtent =
        Envelope(0.0, 0.0, static_cast<double>(xSize), static_cast<double>(ySize));
}

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
        return errorMessage(_("Failed to writeWorldFile. %s"), CPLGetLastErrorMsg());
    }
    return GDALWriteWorldFile(m_path.c_str(), newExt.c_str(), geoTransform) != 0;
}

const Envelope &Raster::extent() const
{
     return m_extent;
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
    return GDALGetDataTypeSizeBytes(dt);
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
    auto data = dynamic_cast<DownloadData*>(threadData);
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
        CPLLockHolderD(&hLock, LOCK_RECURSIVE_MUTEX)

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
    auto requestOptions = data->m_options.asCPLStringList();
    CPLHTTPResult *result = CPLHTTPFetch(url, requestOptions);
    if(result->nStatus != 0 || result->pszErrBuf != nullptr) {
        outMessage(COD_REQUEST_FAILED, result->pszErrBuf);
        CPLHTTPDestroyResult(result);
        return false;
    }

    bool out = File::writeFile(path, result->pabyData,
                               static_cast<size_t>(result->nDataLen));

    CPLHTTPDestroyResult(result);

    return out;
}

bool Raster::cacheArea(const Options &options, const Progress &progress)
{
    if(!isOpened()) {
        outMessage(COD_UNSUPPORTED, "Raster must be opened.");
        return false;
    }
    if(m_type != CAT_RASTER_TMS) {
        outMessage(COD_UNSUPPORTED, "Unsupported type of raster. Mast be web based like TMS, WMS, etc.");
        return false;
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
        outMessage(COD_UNSUPPORTED, _("Zoom level list is empty."));
        return false;
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

bool Raster::createCopy(const std::string &outPath,
                        const Options &options, const Progress &progress)
{
    resetError();
    enum ngsCatalogObjectType dstType = static_cast<enum ngsCatalogObjectType>(
                options.asInt("TYPE", 0));
    std::string newPath(outPath);
    if(!endsWith(newPath, Filter::extension(dstType))) {
        newPath += Filter::extension(dstType);
    }
    auto driver = Filter::getGDALDriver(dstType);
    Options newOptions(options);
    bool strict = options.asBool("STRICT", false);
    newOptions.remove("STRICT");
    newOptions.remove("CREATE_UNIQUE");
    newOptions.remove("NEW_NAME");
    newOptions.remove("OVERWRITE");

    Progress progressIn(progress);
    auto outDS = driver->CreateCopy(outPath.c_str(),
        m_DS, strict, newOptions.asCPLStringList(), ngsGDALProgress, &progressIn);
    bool result = outDS != nullptr;
    GDALClose(outDS);
    return result;
}

bool Raster::moveTo(const std::string &dstPath, const Progress &progress)
{
    close();
    Progress multiProgress(progress);
    multiProgress.setTotalSteps(static_cast<unsigned char>(m_siblingFiles.size() + 1));
    unsigned char counter = 0;
    multiProgress.setStep(counter++);
    auto dstDir = File::getPath(dstPath);
    auto dstName = File::getBaseName(dstPath);

    auto dstFullPath = File::formFileName(dstDir, dstName, File::getExtension(m_path));
    bool result = File::moveFile(m_path, dstFullPath, multiProgress);
    if(!result) {
        return result;
    }
    // TODO: Need check for case there sibling file name not equal raster filw name
    for(const auto &file : m_siblingFiles) {
        multiProgress.setStep(counter++);
        dstFullPath = File::formFileName(dstDir, dstName, File::getExtension(file));
        File::moveFile(file, dstFullPath, multiProgress);
    }
    return result;
}


} // namespace ngs
