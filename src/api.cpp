/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016,2017 NextGIS, <info@nextgis.com>
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
#include "ngstore/api.h"

// stl
#include <iostream>

#include "catalog/catalog.h"
#include "catalog/mapfile.h"
#include "map/mapstore.h"
#include "ngstore/version.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"
#include "util/versionutil.h"


using namespace ngs;

// TODO: Load and tiled vector data
// TODO: Update/Fix unit test. Add GL offscreen rendering GL test
// TODO: Add support to Framebuffer Objects rendering


constexpr const char* HTTP_TIMEOUT = "5";
constexpr const char* HTTP_USE_GZIP = "ON";
constexpr const char* CACHEMAX = "24";

//static CPLString gFilters;

static bool gDebugMode = false;

bool isDebugMode()
{
    return gDebugMode;
}

/* special hook for find EPSG files
static const char *CSVFileOverride( const char * pszInput )
{
    return crash here as we need to duplicate string -> CPLFindFile ("", pszInput);
}
*/

void initGDAL(const char* dataPath, const char* cachePath)
{
    // set config options
    if(dataPath)
        CPLSetConfigOption("GDAL_DATA", dataPath);
    CPLSetConfigOption("GDAL_CACHEMAX", CACHEMAX);
    CPLSetConfigOption("GDAL_HTTP_USERAGENT", NGS_USERAGENT);
    CPLSetConfigOption("CPL_CURL_GZIP", HTTP_USE_GZIP);
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", HTTP_TIMEOUT);
    CPLSetConfigOption("GDAL_DRIVER_PATH", "disabled");
#ifdef NGS_MOBILE // for mobile devices
    CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", "apk");
#endif
    if(cachePath)
        CPLSetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", cachePath);
    if(isDebugMode())
        CPLSetConfigOption("CPL_DEBUG", "ON");

#ifdef _DEBUG
    std::cout << "HTTP user agent set to: " << NGS_USERAGENT << "\n"
              << "GDAL_DATA: " << CPLGetConfigOption("GDAL_DATA", "undefined")
              <<  std::endl;
#endif //_DEBUG

    // register drivers
#ifdef NGS_MOBILE // for mobile devices
    GDALRegister_VRT();
    GDALRegister_GTiff();
    GDALRegister_HFA();
    GDALRegister_PNG();
    GDALRegister_JPEG();
    GDALRegister_MEM();
    RegisterOGRShape();
    RegisterOGRTAB();
    RegisterOGRVRT();
    RegisterOGRMEM();
    RegisterOGRGPX();
    RegisterOGRKML();
    RegisterOGRGeoJSON();
    RegisterOGRGeoPackage();
    RegisterOGRSQLite();
    //GDALRegister_WMS();
#else
    GDALAllRegister();
#endif
    //SetCSVFilenameHook( CSVFileOverride );
}

/**
 * @brief Get library version number as major * 10000 + minor * 100 + rev
 * @param request may be gdal, proj, geos, curl, jpeg, png, zlib, iconv, sqlite3,
 *        openssl, expat, jsonc, tiff, geotiff
 * @return library version number
 */
int ngsGetVersion(const char* request)
{
    return getVersion(request);
}

/**
 * @brief Get library version string
 * @param request may be gdal, proj, geos, curl, jpeg, png, zlib, iconv, sqlite3,
 *        openssl, expat, jsonc, tiff, geotiff
 * @return library version string
 */
const char* ngsGetVersionString(const char* request)
{
    return getVersionString(request);
}

/**
 * @brief init library structures
 * @param options Init library options list:
 * - CACHE_DIR - path to cache directory (mainly for TMS/WMS cache)
 * - SETTINGS_DIR - path to settings directory
 * - GDAL_DATA - path to GDAL data directory (may be skipped on Linux)
 * - DEBUG_MODE ["ON", "OFF"] - May be ON or OFF strings to enable/isable debag mode
 * - LOCALE ["en_US.UTF-8", "de_DE", "ja_JP", ...] - Locale for error messages, etc.
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsInit(char **options)
{
    gDebugMode = CSLFetchBoolean(options, "DEBUG_MODE", 0) == 0 ? false : true;
    const char* dataPath = CSLFetchNameValue(options, "GDAL_DATA");
    const char* cachePath = CSLFetchNameValue(options, "CACHE_DIR");
    const char* settingsPath = CSLFetchNameValue(options, "SETTINGS_DIR");
    CPLSetConfigOption("NGS_SETTINGS_PATH", settingsPath);

#ifdef HAVE_LIBINTL_H
    const char* locale = CSLFetchNameValue(options, "LOCALE");
    //TODO: Do we need std::setlocale(LC_ALL, locale); execution here in library or it will call from programm?
#endif

#ifdef NGS_MOBILE
    if(nullptr == dataPath)
        return returnError(ngsErrorCodes::EC_NOT_SPECIFIED,
                           _("GDAL_PATH option is required"));
#endif

    initGDAL(dataPath, cachePath);

    Catalog::setInstance(new Catalog());
    MapStore::setInstance(new MapStore());

    return ngsErrorCodes::EC_SUCCESS;
}

/**
 * @brief Clean up library structures
 */
void ngsUnInit()
{
    MapStore::setInstance(nullptr);
    Catalog::setInstance(nullptr);
    GDALDestroyDriverManager();
}

/**
 * @brief ngsFreeResources Inform library to free resources as possible
 * @param full If full is true maximum resources will be freed.
 */
void ngsFreeResources(bool full)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr != mapStore) {
        mapStore->freeResources();
    }
    if(full) {
        CatalogPtr catalog = Catalog::getInstance();
        if(catalog) {
            catalog->freeResources();
        }
    }
}

/**
 * @brief Fetches the last error message posted with returnError, CPLError, etc.
 * @return last error message or NULL if no error message present.
 */
const char *ngsGetLastErrorMessage()
{
    return CPLGetLastErrorMsg();
}

//------------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------------

/**
 * @brief Query name and type of child objects for provided path and filter.
 * @param path The path inside catalog in form ngc://Local connections/tmp
 * @param filter Only objects correspondent to provided filter will be return.
 * The filter is combination of enum ngsCatalogObjectType and subtype according
 * object type.
 * @return Array of ngsCatlogObjectInfo structures. Caller mast free this array
 * after using with free or CPLFree method.
 */
ngsCatalogObjectInfo* ngsCatalogObjectQuery(const char *path, int filter)
{
    CatalogPtr catalog = Catalog::getInstance();
    // Create filter class from filter value.
    Filter objectFilter(static_cast<enum ngsCatalogObjectType>(filter));
    ObjectPtr object = catalog->getObject(path);
    if(!object) {
        return nullptr;
    }

    ngsCatalogObjectInfo* output = nullptr;
    size_t outputSize = 0;
    ObjectContainer* const container = ngsDynamicCast(ObjectContainer, object);
    if(!container) {
        if(!objectFilter.canDisplay(object))
            return nullptr;
        output = static_cast<ngsCatalogObjectInfo*>(
                    CPLMalloc(sizeof(ngsCatalogObjectInfo) * 2));
        output[0] = {object->getName(), object->getType()};
        output[1] = {nullptr, -1};
        return output;
    }

    if(!container->hasChildren())
        return nullptr;

    auto children = container->getChildren();
    if(children.empty())
        return nullptr;

    for(const auto &child : children) {
        if(objectFilter.canDisplay(child)) {
            outputSize++;
            output = static_cast<ngsCatalogObjectInfo*>(CPLRealloc(output,
                                sizeof(ngsCatalogObjectInfo) * (outputSize + 1)));
            output[outputSize - 1] = {
                    child->getName(), child->getType()};
        }
    }
    if(outputSize > 0) {
        output[outputSize] = {nullptr, -1};
    }
    return output;
}

/**
 * @brief Delete catalog object on specified path
 * @param path The path inside catalog in form ngc://Local connections/tmp
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsCatalogObjectDelete(const char *path)
{
    CatalogPtr catalog = Catalog::getInstance();
    ObjectPtr object = catalog->getObject(path);
    // Check can delete
    if(object && object->canDestroy())
        return object->destroy() ? ngsErrorCodes::EC_SUCCESS :
                                   ngsErrorCodes::EC_DELETE_FAILED;
    return errorMessage(ngsErrorCodes::EC_UNSUPPORTED,
                       _("The path cannot be deleted (write protected, locked, etc.)"));
}

/**
 * @brief Create new catalog object
 * @param path The path inside catalog in form ngc://Local connections/tmp
 * @param name The new object name
 * @param options The array of create object options. Caller mast free this
 * array after function finishes. The common values are:
 * TYPE (required) - The new object type from enum ngsCatalogObjectType
 * CREATE_UNIQUE [ON, OFF] - If name already exists in container, make it unique
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsCatalogObjectCreate(const char *path, const char* name, char **options)
{
    CatalogPtr catalog = Catalog::getInstance();
    Options createOptions(options);
    enum ngsCatalogObjectType type = static_cast<enum ngsCatalogObjectType>(
                createOptions.getIntOption("TYPE", CAT_UNKNOWN));
    createOptions.removeOption("TYPE");
    ObjectPtr object = catalog->getObject(path);
    ObjectContainer * const container = ngsDynamicCast(ObjectContainer, object);
    // Check can create
    if(nullptr != container && container->canCreate(type))
        return container->create(type, name, createOptions) ?
                    ngsErrorCodes::EC_SUCCESS : ngsErrorCodes::EC_CREATE_FAILED;

    return errorMessage(ngsErrorCodes::EC_UNSUPPORTED,
                        _("Cannot create such object type (%d) in path: %s"),
                        type, path);
}

/**
 * @brief Find catalog path (i.e. ngc://Local connections/tmp) correspondent
 * system path (i.e. /home/user/tmp
 * @param path System path
 * @return Catalog path
 */
const char *ngsCatalogPathFromSystem(const char *path)
{
    CatalogPtr catalog = Catalog::getInstance();
    ObjectPtr object = catalog->getObjectByLocalPath(path);
    if(object) {
        static CPLString fullName = object->getFullName();
        return fullName;
    }
    return "";
}

/**
 * @brief Copy or move source dataset to destination dataset
 * @param srcPath The part to source dataset
 * @param dstPath The path to destination dataset. Should be container which
 * is ready to accept source dataset types.
 * @param options The options key-value array specific to operation and
 * destination dataset. TODO: Add common key and values.
 * @param callback The callback function to report or cancel process.
 * @param callbackData The callback function data.
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsCatalogObjectLoad(const char *srcPath, const char *dstPath,
                         char **options, ngsProgressFunc callback,
                         void *callbackData)
{
    CatalogPtr catalog = Catalog::getInstance();
    Progress progress(callback, callbackData);

    return ngsErrorCodes::EC_SUCCESS;
}

/**
 * @brief Rename catalog object
 * @param path The path inside catalog in form ngc://Local connections/tmp
 * @param newName The new object name. The name should be unique inside object
 * parent container
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsCatalogObjectRename(const char *path, const char *newName)
//{
//    CatalogPtr catalog = Catalog::getInstance();
//    return ngsErrorCodes::EC_SUCCESS;
//}

//------------------------------------------------------------------------------
// Map
//------------------------------------------------------------------------------

/**
 * @brief ngsCreateMap Create new empty map
 * @param name Map name
 * @param description Map description
 * @param epsg EPSG code
 * @param minX minimum X coordinate
 * @param minY minimum Y coordinate
 * @param maxX maximum X coordinate
 * @param maxY maximum Y coordinate
 * @return 0 if create failed or map id.
 */
//int ngsMapCreate(const char* name, const char* description,
//                 unsigned short epsg, double minX, double minY,
//                 double maxX, double maxY)
//{
//    // for this API before work with map datastore must be open
//    if(nullptr == gDataStore)
//        return ngsErrorCodes::EC_CREATE_FAILED; // FIXME: must return 0
//    initMapStore();
//    return gMapStore->createMap (name, description, epsg,
//                                 minX, minY, maxX, maxY, gDataStore);
//}

/**
 * @brief ngsOpenMap Open existing map from file
 * @param path Path to map file inside catalog in form ngc://some path/
 * @return 0 if open failed or map id.
 */
unsigned char ngsMapOpen(const char *path)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore)
        return MapStore::invalidMapId();
    CatalogPtr catalog = Catalog::getInstance();
    ObjectPtr object = catalog->getObject(path);
    MapFile * const mapFile = ngsDynamicCast(MapFile, object);
    return mapStore->openMap(mapFile);
}

/**
 * @brief ngsSaveMap Save map to file
 * @param mapId Map id to save
 * @param path Path to store map data
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapSave(unsigned char mapId, const char *path)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        return MapStore::invalidMapId();
    }
    CatalogPtr catalog = Catalog::getInstance();
    ObjectPtr mapFileObject = catalog->getObject(path);
    MapFile * mapFile;
    if(mapFileObject) {
        mapFile = ngsDynamicCast(MapFile, mapFileObject);
    }
    else { // create new MapFile
        const char* newPath = CPLResetExtension(path, MapFile::getExtension());
        const char* saveFolder = CPLGetPath(newPath);
        const char* saveName = CPLGetFilename(newPath);
        ObjectPtr object = catalog->getObject(saveFolder);
        ObjectContainer * const container = ngsDynamicCast(ObjectContainer, object);
        mapFile = new MapFile(container, saveName,
                        CPLFormFilename(object->getPath(), saveName, nullptr));
        mapFileObject = ObjectPtr(mapFile);
    }

    if(!mapStore->saveMap(mapId, mapFile)) {
        return ngsErrorCodes::EC_SAVE_FAILED;
    }

    return ngsErrorCodes::EC_SUCCESS;
}

/**
 * @brief ngsMapClose Close map and free resources
 * @param mapId Map id to close
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapClose(unsigned char mapId)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore)
        return errorMessage(ngsErrorCodes::EC_CLOSE_FAILED,
                            _("MapStore is not initialized"));
    return mapStore->closeMap(mapId) ? ngsErrorCodes::EC_SUCCESS :
                                       ngsErrorCodes::EC_CLOSE_FAILED;
}

/**
 * @brief ngsMapInit Initialize map. It depends on map what to initialize.
 * @param mapId Map id
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsMapInit(unsigned char mapId)
//{
//    initMapStore();
//    return gMapStore->initMap (mapId);
//}


/**
 * @brief ngsInitDataStore Open or create data store. All datastore functgions
 * will work with this datastore object until new one willn ot open.
 * @param path Path to create datastore (geopackage database name)
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsDataStoreInit(const char *path)
//{
//    if(gDataStore && gDataStore->path().compare (path) == 0)
//        return ngsErrorCodes::EC_SUCCESS;
//    gDataStore = DataStore::openOrCreate(path);
//    if(nullptr == gDataStore)
//        return ngsErrorCodes::EC_OPEN_FAILED;
//    return ngsErrorCodes::EC_SUCCESS;
//}


/**
 * @brief Delete storage directory and cache (optional)
 * @param path Path to storage
 * @param cachePath Path to cache (may be NULL)
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsDataStoreDestroy(const char *path, const char *cachePath)
//{
//    if(nullptr == path)
//        return ngsErrorCodes::EC_INVALID;
//    gMapStore.reset ();
//    if(cachePath){
//        if(CPLUnlinkTree(cachePath) != 0){
//            return ngsErrorCodes::EC_DELETE_FAILED;
//        }
//    }
//    ngsDataStoreInit(path);
//    return gDataStore->destroy ();
//}

/**
 * @brief Create remote TMS Raster
 * @param url URL to TMS tiles with replacement variables, of the format ${x}, ${y}, etc.
 * @param name Layer name, only alpha, numeric and underline
 * @param alias Layer alias name. User readable text
 * @param copyright Copyright text, link, etc. (optional)
 * @param epsg EPSG code
 * @param z_min Minimum zoom level
 * @param z_max Maximum zoom level. If equal 0 will be set to 18
 * @param y_origin_top If true - OSGeo TMS, else - Slippy map
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsCreateRemoteTMSRaster(const char *url, const char *name, const char *alias,
//                             const char *copyright, int epsg, int z_min, int z_max,
//                             bool y_origin_top)
//{
//    if(z_max == 0)
//        z_max = 18;

//    if(gDataStore)
//        return gDataStore->createRemoteTMSRaster(url, name, alias, copyright,
//                                            epsg, z_min, z_max, y_origin_top);

//    return ngsErrorCodes::EC_CREATE_FAILED;
//}

/**
 * @brief Sete map size in pixels
 * @param mapId Map id received from create or open map functions
 * @param width Output image width
 * @param height Output image height
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsMapSetSize(unsigned char mapId, int width, int height, int isYAxisInverted)
//{
//    initMapStore();
//    return gMapStore->setMapSize (mapId, width, height, isYAxisInverted);
//}


/**
 * @brief Set notify function executed on some library events
 * @param Callback function pointer (not free by library)
 */
//void ngsSetNotifyFunction(ngsNotifyFunc callback)
//{
//    if(nullptr != gDataStore)
//        gDataStore->setNotifyFunc (callback);
//    initMapStore();
//    gMapStore->setNotifyFunc (callback);
//}

/**
 * @brief ngsDrawMap Start drawing map in specified (in ngsInitMap) extent
 * @param mapId Map id received from create or open map functions
 * @param state Draw state (NORMAL, PRESERVED, REDRAW)
 * @param callback Progress function
 * @param callbackData Progress function arguments
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsMapDraw(unsigned char mapId, enum ngsDrawState state,
//               ngsProgressFunc callback, void* callbackData)
//{
//    initMapStore();
//    return gMapStore->drawMap (mapId, state, callback, callbackData);
//}

/**
 * @brief ngsGetMapBackgroundColor Map background color
 * @param mapId Map id received from create or open map functions
 * @return map background color struct
 */
//ngsRGBA ngsMapGetBackgroundColor(unsigned char mapId)
//{
//    initMapStore();
//    return gMapStore->getMapBackgroundColor (mapId);
//}

/**
 * @brief ngsSetMapBackgroundColor set specified by name map background color
 * @param mapId Map id received from create or open map functions
 * @param R red
 * @param G green
 * @param B blue
 * @param A alpha
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsMapSetBackgroundColor(unsigned char mapId, unsigned char R, unsigned char G,
//                             unsigned char B, unsigned char A)
//{
//    initMapStore();
//    return gMapStore->setMapBackgroundColor (mapId, {R, G, B, A});
//}

/**
 * @brief ngsMapSetCenter Set new map center coordinates
 * @param mapId Map id
 * @param x X coordinate
 * @param y Y coordinate
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsMapSetCenter(unsigned char mapId, double x, double y)
//{
//    initMapStore();
//    return gMapStore->setMapCenter(mapId, x, y);
//}

/**
 * @brief ngsMapGetCenter Get map center for current view (extent)
 * @param mapId Map id
 * @return Coordintate structure. If error occured all coordinates set to 0.0
 */
//ngsCoordinate ngsMapGetCenter(unsigned char mapId)
//{
//    initMapStore();
//    return gMapStore->getMapCenter(mapId);
//}

/**
 * @brief ngsMapSetScale Set current map scale
 * @param mapId Map id
 * @param scale value to set
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsMapSetScale(unsigned char mapId, double scale)
//{
//    initMapStore();
//    return gMapStore->setMapScale(mapId, scale);
//}

/**
 * @brief ngsMapGetScale Return current map scale
 * @param mapId Map id
 * @return Current map scale or 1
 */
//double ngsMapGetScale(unsigned char mapId)
//{
//    initMapStore();
//    return gMapStore->getMapScale(mapId);
//}

/**
 * @brief ngsLoad
 * @param name Destination dataset name
 * @param path Path to file in OS
 * @param subDatasetName Layer name, if datasource on path has several layers
 * @param options various options dependent on destionation datasource and other
 * things. Options can be get by executing @ngsDataStoreGetOptions function.
 * @param callback Progress function
 * @param callbackData Progress function arguments
 * @return  Load task id or 0 if error occured. This id can be used to get some
 * additional information about the task.
 */
//unsigned int ngsDataStoreLoad(const char* name, const char *path,
//                              const char *subDatasetName, const char** options,
//                              ngsProgressFunc callback, void *callbackData)
//{
//    if(nullptr != gDataStore) {
//        CPLString sName(name);
//        CPLString sPath(path);
//        CPLString sSubDatasetName(subDatasetName);
//        return gDataStore->loadDataset(
//                sName, sPath, sSubDatasetName, options, callback, callbackData);
//    }
//    return 0;
//}

//int ngsMapCreateLayer(unsigned char mapId, const char *name, const char *path)
//{
//    ngsDataStoreInit ( CPLGetDirname (path) );
//    if(nullptr == gDataStore)
//        return ngsErrorCodes::EC_CREATE_FAILED;
//    DatasetPtr dataset = gDataStore->getDataset ( CPLGetBasename (path) );
//    initMapStore();
//    MapPtr map = gMapStore->getMap (mapId);
//    if(nullptr == map || nullptr == dataset)
//        return ngsErrorCodes::EC_CREATE_FAILED;
//    return map->createLayer (name, dataset);

//}

/**
 * @brief ngsMapSetRotate Set map rotate
 * @param mapId Map id
 * @param dir Rotate direction. May be X, Y or Z
 * @param rotate value to set
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
//int ngsMapSetRotate(unsigned char mapId, ngsDirection dir, double rotate)
//{
//    initMapStore();
//    return gMapStore->setMapRotate (mapId, dir, rotate);
//}

/**
 * @brief ngsMapGetRotate Return map rotate value
 * @param mapId Map id
 * @param dir Rotate direction. May be X, Y or Z
 * @return rotate value or 0 if error occured
 */
//double ngsMapGetRotate(unsigned char mapId, ngsDirection dir)
//{
//    initMapStore();
//    return gMapStore->getMapRotate (mapId, dir);
//}

/**
 * @brief ngsMapGetCoordinate Georpaphic coordinates for display positon
 * @param mapId Map id
 * @param x X position
 * @param y Y position
 * @return Georpaphic coordinates
 */
//ngsCoordinate ngsMapGetCoordinate(unsigned char mapId, double x, double y)
//{
//    initMapStore();
//    return gMapStore->getMapCoordinate (mapId, x, y);
//}

/**
 * @brief ngsDisplayGetPosition Display position for geographic coordinates
 * @param mapId Map id
 * @param x X coordinate
 * @param y Y coordinate
 * @return Display position
 */
//ngsPosition ngsDisplayGetPosition(unsigned char mapId, double x, double y)
//{
//    initMapStore();
//    return gMapStore->getDisplayPosition (mapId, x, y);
//}

/**
 * @brief ngsMapGetDistance Map distance from display length
 * @param mapId Map id
 * @param w Width
 * @param h Height
 * @return ngsCoordinate where X distance along x axis and Y along y axis
 */
//ngsCoordinate ngsMapGetDistance(unsigned char mapId, double w, double h)
//{
//    initMapStore();
//    return gMapStore->getMapDistance (mapId, w, h);
//}

/**
 * @brief ngsDisplayGetLength Display length from map distance
 * @param mapId Map id
 * @param w Width
 * @param h Height
 * @return ngsPosition where X length along x axis and Y along y axis
 */
//ngsPosition ngsDisplayGetLength(unsigned char mapId, double w, double h)
//{
//    initMapStore();
//    return gMapStore->getDisplayLength (mapId, w, h);
//}

/**
 * @brief ngsDataStoreGetLoadTaskInfo Report loading task status
 * @param taskId Task id
 * @return ngsLoadTaskInfo structure
 */
//ngsLoadTaskInfo ngsDataStoreGetLoadTaskInfo(unsigned int taskId)
//{
//    if(nullptr != gDataStore)
//        return gDataStore->getLoadTaskInfo (taskId);
//    return {nullptr, nullptr, nullptr, ngsErrorCodes::EC_INVALID};
//}

/**
 * @brief ngsDataStoreGetOptions Report supported options in xml string
 * @param optionType Option to report
 * @return xml string or nullptr
 */
//const char *ngsDataStoreGetOptions(ngsDataStoreOptionsTypes optionType)
//{
//    if(nullptr != gDataStore)
//        return gDataStore->getOptions (optionType);
//    return nullptr;
//}

//const char *ngsGetFilters(unsigned int flags, unsigned int mode, const char *separator)
//{
//    gFilters.Clear ();

//    // cannot combine DT_*_ALL with othe flags
//    if(flags & DT_VECTOR_ALL && flags != DT_VECTOR_ALL)
//        return gFilters;
//    if(flags & DT_RASTER_ALL && flags != DT_RASTER_ALL)
//        return gFilters;
//    if(flags & DT_VECTOR_ALL)
//        gFilters = "Vector datasets (";
//    if(flags & DT_RASTER_ALL)
//        gFilters = "Raster datasets (";

//    for( int iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
//    {
//        GDALDriverH hDriver = GDALGetDriver(iDr);

//        char** papszMD = GDALGetMetadata( hDriver, NULL );

//        if( (flags & DT_RASTER &&
//            !CSLFetchBoolean( papszMD, GDAL_DCAP_RASTER, FALSE ) ) &&
//            (flags & DT_VECTOR &&
//            !CSLFetchBoolean( papszMD, GDAL_DCAP_VECTOR, FALSE ) ) &&
//            (flags & DT_GNM &&
//            !CSLFetchBoolean( papszMD, GDAL_DCAP_GNM, FALSE ) ) )
//            continue;

//        if( flags & DT_RASTER_ALL &&
//            !CSLFetchBoolean( papszMD, GDAL_DCAP_RASTER, FALSE ) )
//            continue;

//        if( flags & DT_VECTOR_ALL &&
//            !CSLFetchBoolean( papszMD, GDAL_DCAP_VECTOR, FALSE ) )
//            continue;

//        if( mode & FM_WRITE &&
//            (!CSLFetchBoolean( papszMD, GDAL_DCAP_CREATE, FALSE ) ||
//             !CSLFetchBoolean( papszMD, GDAL_DCAP_CREATECOPY, FALSE )) )
//            continue;

//        if( flags & DT_SERVICE &&
//                EQUAL(CSLFetchNameValueDef(papszMD, GDAL_DMD_CONNECTION_PREFIX,
//                                           ""), "") )
//            continue;

//        if(! (flags & DT_SERVICE) &&
//                !EQUAL(CSLFetchNameValueDef(papszMD, GDAL_DMD_CONNECTION_PREFIX,
//                                           ""), "") )
//            continue;

//        const char* longName = CSLFetchNameValue(papszMD, GDAL_DMD_LONGNAME);
//        const char* ext = CSLFetchNameValue(papszMD, GDAL_DMD_EXTENSION);

//        if( (flags & DT_VECTOR_ALL || flags & DT_RASTER_ALL) && nullptr != ext &&
//                !EQUAL(ext, "")) {
//            gFilters += " *." + CPLString(ext);
//        }
//        else if(nullptr != longName && nullptr != ext && !EQUAL(longName, "") &&
//                !EQUAL(ext, ""))
//        {
//            if(gFilters.empty ())
//                gFilters += CPLString(longName) + " (*." + CPLString(ext) + ")";
//            else
//                gFilters += separator + CPLString(longName) + " (*." + CPLString(ext) + ")";
//        }
//    }

//    if( flags & DT_VECTOR_ALL || flags & DT_RASTER_ALL) {
//        gFilters += ")";
//    }

//    return gFilters;
//}
