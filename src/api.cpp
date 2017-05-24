/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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
#include "ds/simpledataset.h"
#include "map/mapstore.h"
#include "ngstore/catalog/filter.h"
#include "ngstore/version.h"
#include "ngstore/util/constants.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/settings.h"
#include "util/versionutil.h"

using namespace ngs;

// TODO: Tile vector data on load

constexpr const char* HTTP_TIMEOUT = "2";
constexpr const char* HTTP_USE_GZIP = "ON";
constexpr const char* CACHEMAX = "24";

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
    Settings& settings = Settings::instance();
    // set config options
    if(dataPath) {
        CPLSetConfigOption("GDAL_DATA", dataPath);
    }

    CPLSetConfigOption("GDAL_CACHEMAX",
                       settings.getString("common/cachemax", CACHEMAX));
    CPLSetConfigOption("GDAL_HTTP_USERAGENT",
                       settings.getString("http/useragent", NGS_USERAGENT));
    CPLSetConfigOption("CPL_CURL_GZIP",
                       settings.getString("http/use_gzip", HTTP_USE_GZIP));
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT",
                       settings.getString("http/timeout", HTTP_TIMEOUT));
    CPLSetConfigOption("GDAL_DRIVER_PATH", "disabled");
#ifdef NGS_MOBILE // for mobile devices
    CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS",
                       settings.getString("gdal/CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", "apk"));
#endif
    if(cachePath) {
        CPLSetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", cachePath);
    }
    if(isDebugMode()) {
        CPLSetConfigOption("CPL_DEBUG", "ON");
        CPLSetConfigOption("CPL_CURL_VERBOSE", "ON");
    }

    CPLSetConfigOption("CPL_ZIP_ENCODING",
                       settings.getString("common/zip_encoding", "CP866"));
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
 * @brief ngsGetVersion Get library version number as major * 10000 + minor * 100 + rev
 * @param request may be gdal, proj, geos, curl, jpeg, png, zlib, iconv, sqlite3,
 *        openssl, expat, jsonc, tiff, geotiff
 * @return library version number
 */
int ngsGetVersion(const char* request)
{
    return getVersion(request);
}

/**
 * @brief ngsGetVersionString Get library version string
 * @param request may be gdal, proj, geos, curl, jpeg, png, zlib, iconv, sqlite3,
 *        openssl, expat, jsonc, tiff, geotiff
 * @return library version string
 */
const char* ngsGetVersionString(const char* request)
{
    return getVersionString(request);
}

/**
 * @brief ngsInit Init library structures
 * @param options Init library options list:
 * - CACHE_DIR - path to cache directory (mainly for TMS/WMS cache)
 * - SETTINGS_DIR - path to settings directory
 * - GDAL_DATA - path to GDAL data directory (may be skipped on Linux)
 * - DEBUG_MODE ["ON", "OFF"] - May be ON or OFF strings to enable/isable debag mode
 * - LOCALE ["en_US.UTF-8", "de_DE", "ja_JP", ...] - Locale for error messages, etc.
 * - NUM_THREADS - number theads in various functions (a positive number or "ALL_CPUS")
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsInit(char **options)
{
    gDebugMode = CSLFetchBoolean(options, "DEBUG_MODE", 0) == 0 ? false : true;
    const char* dataPath = CSLFetchNameValue(options, "GDAL_DATA");
    const char* cachePath = CSLFetchNameValue(options, "CACHE_DIR");
    const char* settingsPath = CSLFetchNameValue(options, "SETTINGS_DIR");
    CPLSetConfigOption("NGS_SETTINGS_PATH", settingsPath);

    // Number threads
    const char* numThreads = CSLFetchNameValueDef(options, "NUM_THREADS", nullptr);
    if(!numThreads) {
        int cpuCount = CPLGetNumCPUs() - 1;
        if(cpuCount < 2)
            cpuCount = 1;
        numThreads = CPLSPrintf("%d", cpuCount);
    }
    CPLSetConfigOption("GDAL_NUM_THREADS", numThreads);


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

    return ngsCode::COD_SUCCESS;
}

/**
 * @brief ngsUnInit Clean up library structures
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
 * @brief ngsGetLastErrorMessage Fetches the last error message posted with
 * returnError, CPLError, etc.
 * @return last error message or NULL if no error message present.
 */
const char *ngsGetLastErrorMessage()
{
    return getLastError();
}

/**
 * @brief ngsAddNotifyFunction Add function triggered on some events
 * @param function Function executed on event occured
 * @param notifyTypes The OR combination of ngsChangeCode
 */
void ngsAddNotifyFunction(ngsNotifyFunc function, int notifyTypes)
{
    Notify::instance().addNotifyReceiver(function, notifyTypes);
}

/**
 * @brief ngsRemoveNotifyFunction Remove function. No events will be occured.
 * @param function The function to remove
 */
void ngsRemoveNotifyFunction(ngsNotifyFunc function)
{
    Notify::instance().deleteNotifyReceiver(function);
}

//------------------------------------------------------------------------------
// GDAL proxy functions
//------------------------------------------------------------------------------

const char *ngsGetCurrentDirectory()
{
    return CPLGetCurrentDir();
}

char **ngsAddNameValue(char **list, const char *name, const char *value)
{
    return CSLAddNameValue(list, name, value);
}

void ngsDestroyList(char **list)
{
    CSLDestroy(list);
}

const char *ngsFormFileName(const char *path, const char *name,
                            const char *extension)
{
    return CPLFormFilename(path, name, extension);
}

void ngsFree(void *pointer)
{
    CPLFree(pointer);
}

//------------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------------

ngsCatalogObjectInfo *catalogObjectQuery(CatalogObjectH object,
                                         const Filter& objectFilter)
{
    Object* catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(ngsCode::COD_INVALID, _("The object handle is null"));
        return nullptr;
    }
    ObjectPtr catalogObjectPointer = catalogObject->getPointer();

    ngsCatalogObjectInfo* output = nullptr;
    size_t outputSize = 0;
    ObjectContainer* const container = dynamic_cast<ObjectContainer*>(catalogObject);
    if(!container) {
        if(!objectFilter.canDisplay(catalogObjectPointer)) {
            return nullptr;
        }

        output = static_cast<ngsCatalogObjectInfo*>(
                    CPLMalloc(sizeof(ngsCatalogObjectInfo) * 2));
        output[0] = {catalogObject->getName(), catalogObject->getType(), catalogObject};
        output[1] = {nullptr, -1, nullptr};
        return output;
    }

    if(!container->hasChildren()) {
        if(container->getType() == ngsCatalogObjectType::CAT_CONTAINER_SIMPLE) {
            SimpleDataset * const simpleDS = dynamic_cast<SimpleDataset*>(container);
            output = static_cast<ngsCatalogObjectInfo*>(
                        CPLMalloc(sizeof(ngsCatalogObjectInfo) * 2));
            output[0] = {catalogObject->getName(), simpleDS->getSubType(), catalogObject};
            output[1] = {nullptr, -1, nullptr};
            return output;
        }
        return nullptr;
    }

    auto children = container->getChildren();
    if(children.empty()) {
        return nullptr;
    }

    for(const auto &child : children) {
        if(objectFilter.canDisplay(child)) {
            outputSize++;
            output = static_cast<ngsCatalogObjectInfo*>(CPLRealloc(output,
                                sizeof(ngsCatalogObjectInfo) * (outputSize + 1)));

            if(child->getType() == ngsCatalogObjectType::CAT_CONTAINER_SIMPLE) {
                SimpleDataset * const simpleDS = ngsDynamicCast(SimpleDataset, child);
                output[outputSize - 1] = {child->getName(),
                                          simpleDS->getSubType(),
                                          simpleDS->getInternalObject().get()};
            }
            else {
                output[outputSize - 1] = {child->getName(),
                                          child->getType(), child.get()};
            }
        }
    }
    if(outputSize > 0) {
        output[outputSize] = {nullptr, -1, nullptr};
    }
    return output;
}

/**
 * @brief ngsCatalogObjectQuery Queries name and type of child objects for
 * provided path and filter
 * @param object The handle of catalog object
 * @param filter Only objects correspondent to provided filter will be return
 * @return Array of ngsCatlogObjectInfo structures. Caller mast free this array
 * after using with ngsFree method
 */
ngsCatalogObjectInfo* ngsCatalogObjectQuery(CatalogObjectH object, int filter)
{
    // Create filter class from filter value.
    Filter objectFilter(static_cast<enum ngsCatalogObjectType>(filter));

    return catalogObjectQuery(object, objectFilter);
}

/**
 * @brief ngsCatalogObjectQueryMultiFilter Queries name and type of child objects for
 * provided path and filters
 * @param object The handle of catalog object
 * @param filter Only objects correspondent to provided filters will be return.
 * User mast delete filters array manually
 * @param filterCount The filters count
 * @return Array of ngsCatlogObjectInfo structures. Caller mast free this array
 * after using with ngsFree method
 */
ngsCatalogObjectInfo *ngsCatalogObjectQueryMultiFilter(CatalogObjectH object,
                                                       int *filters,
                                                       int filterCount)
{
    // Create filter class from filter value.
    MultiFilter objectFilter;
    for(int i = 0; i < filterCount; ++i) {
        objectFilter.addType(static_cast<enum ngsCatalogObjectType>(filters[i]));
    }

    return catalogObjectQuery(object, objectFilter);
}

/**
 * @brief ngsCatalogObjectDelete Deletes catalog object on specified path
 * @param object The handle of catalog object
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsCatalogObjectDelete(CatalogObjectH object)
{
    Object* catalogObject = static_cast<Object*>(object);
    if(!catalogObject)
        return errorMessage(ngsCode::COD_INVALID, _("The object handle is null"));

    // Check can delete
    if(catalogObject->canDestroy())
        return catalogObject->destroy() ? ngsCode::COD_SUCCESS :
                                   ngsCode::COD_DELETE_FAILED;
    return errorMessage(ngsCode::COD_UNSUPPORTED,
                       _("The path cannot be deleted (write protected, locked, etc.)"));
}

/**
 * @brief ngsCatalogObjectCreate Creates new catalog object
 * @param object The handle of catalog object
 * @param name The new object name
 * @param options The array of create object options. Caller mast free this
 * array after function finishes. The common values are:
 * TYPE (required) - The new object type from enum ngsCatalogObjectType
 * CREATE_UNIQUE [ON, OFF] - If name already exists in container, make it unique
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsCatalogObjectCreate(CatalogObjectH object, const char* name, char **options)
{
    Object* catalogObject = static_cast<Object*>(object);
    if(!catalogObject)
        return errorMessage(ngsCode::COD_INVALID, _("The object handle is null"));

    Options createOptions(options);
    enum ngsCatalogObjectType type = static_cast<enum ngsCatalogObjectType>(
                createOptions.getIntOption("TYPE", CAT_UNKNOWN));
    createOptions.removeOption("TYPE");
    ObjectContainer * const container = dynamic_cast<ObjectContainer*>(catalogObject);
    // Check can create
    if(nullptr != container && container->canCreate(type))
        return container->create(type, name, createOptions) ?
                    ngsCode::COD_SUCCESS : ngsCode::COD_CREATE_FAILED;

    return errorMessage(ngsCode::COD_UNSUPPORTED,
                        _("Cannot create such object type (%d) in path: %s"),
                        type, catalogObject->getFullName().c_str());
}

/**
 * @brief ngsCatalogPathFromSystem Finds catalog path
 * (i.e. ngc://Local connections/tmp) correspondent system path (i.e. /home/user/tmp
 * @param path System path
 * @return Catalog path
 */
const char *ngsCatalogPathFromSystem(const char *path)
{
    CatalogPtr catalog = Catalog::getInstance();
    ObjectPtr object = catalog->getObjectByLocalPath(path);
    if(object) {
        return object->getFullName();
    }
    return "";
}

/**
 * @brief ngsCatalogObjectLoad Copies or move source dataset to destination dataset
 * @param srcObject The handle of source catalog object
 * @param dstObject The handle of destination destination catalog object.
 * Should be container which is ready to accept source dataset types.
 * @param options The options key-value array specific to operation and
 * destination dataset. The load options can be fetched via
 * ngsCatalogObjectOptions() function. Also load options can combine with
 * layer create options.
 * @param callback The callback function to report or cancel process.
 * @param callbackData The callback function data.
 * @return ngsErrorCode value - EC_SUCCESS if everything is OK
 */
int ngsCatalogObjectLoad(CatalogObjectH srcObject, CatalogObjectH dstObject,
                         char **options, ngsProgressFunc callback,
                         void *callbackData)
{
    Object* srcCatalogObject = static_cast<Object*>(srcObject);
    Object* dstCatalogObject = static_cast<Object*>(dstObject);
    if(!srcCatalogObject || !dstCatalogObject) {
        return errorMessage(ngsCode::COD_INVALID, _("The object handle is null"));
    }

    ObjectPtr srcCatalogObjectPointer = srcCatalogObject->getPointer();
    ObjectPtr dstCatalogObjectPointer = dstCatalogObject->getPointer();

    Progress progress(callback, callbackData);
    Options loadOptions(options);
    bool move = loadOptions.getBoolOption("MOVE", false);
    loadOptions.removeOption("MOVE");

    if(move && !srcCatalogObjectPointer->canDestroy()) {
        return  errorMessage(ngsCode::COD_MOVE_FAILED,
                             _("Cannot move source dataset '%s'"),
                             srcCatalogObjectPointer->getFullName().c_str());
    }

    if(srcCatalogObjectPointer->getType() == ngsCatalogObjectType::CAT_CONTAINER_SIMPLE) {
        SimpleDataset * const dataset = dynamic_cast<SimpleDataset*>(srcCatalogObject);
        dataset->hasChildren();
        srcCatalogObjectPointer = dataset->getInternalObject();
    }

    if(!srcCatalogObjectPointer) {
        return errorMessage(ngsCode::COD_INVALID,
                            _("Source dataset type is incompatible"));
    }

    Dataset * const dstDataset = dynamic_cast<Dataset*>(dstCatalogObject);
    if(nullptr == dstDataset) {
        return move ? ngsCode::COD_MOVE_FAILED :
                      ngsCode::COD_COPY_FAILED;
    }
    dstDataset->hasChildren();

    // Check can paster
    if(dstDataset->canPaste(srcCatalogObjectPointer->getType())) {
        return dstDataset->paste(srcCatalogObjectPointer, move, loadOptions, progress);
    }

    return errorMessage(move ? ngsCode::COD_MOVE_FAILED :
                               ngsCode::COD_COPY_FAILED,
                        _("Destination dataset '%s' is not container or cannot accept source dataset '%s'"),
                        dstCatalogObjectPointer->getFullName().c_str(),
                        srcCatalogObjectPointer->getFullName().c_str());

}

/**
 * @brief ngsCatalogObjectRename Renames catalog object
 * @param object The handle of catalog object
 * @param newName The new object name. The name should be unique inside object
 * parent container
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsCatalogObjectRename(CatalogObjectH object, const char *newName)
{
    Object* catalogObject = static_cast<Object*>(object);
    if(!catalogObject)
        return errorMessage(ngsCode::COD_INVALID, _("The object handle is null"));

    if(!catalogObject->canRename())
        return errorMessage(ngsCode::COD_RENAME_FAILED,
                            _("Cannot rename catalog object '%s' to '%s'"),
                            catalogObject->getFullName().c_str(), newName);

    return catalogObject->rename(newName) ? ngsCode::COD_SUCCESS :
                                     ngsCode::COD_RENAME_FAILED;
}

/**
 * @brief ngsCatalogObjectOptions Queries catalog object options.
 * @param object The handle of catalog object
 * @param optionType The one of ngsOptionTypes enum values:
 * OT_CREATE_DATASOURCE, OT_CREATE_RASTER, OT_CREATE_LAYER,
 * OT_CREATE_LAYER_FIELD, OT_OPEN, OT_LOAD
 * @return Options description in xml form.
 */
const char* ngsCatalogObjectOptions(CatalogObjectH object, int optionType)
{
    if(!object) {
        errorMessage(ngsCode::COD_INVALID, _("The object handle is null"));
        return "";
    }
    Object* catalogObject = static_cast<Object*>(object);
    if(!Filter::isDatabase(catalogObject->getType())) {
        errorMessage(ngsCode::COD_INVALID,
                            _("The input object not a dataset. The type is %d. Options query not supported"),
                     catalogObject->getType());
        return "";
    }

    Dataset * const dataset = dynamic_cast<Dataset*>(catalogObject);
    if(nullptr != dataset) {
        errorMessage(ngsCode::COD_INVALID,
                            _("The input object not a dataset. Options query not supported"));
        return "";
    }
    enum ngsOptionType enumOptionType = static_cast<enum ngsOptionType>(optionType);

    return dataset->getOptions(enumOptionType);
}

/**
 * @brief ngsCatalogObjectGet Gets catalog object handle by path
 * @param path Path to the catalog object
 * @return handel or null
 */
CatalogObjectH ngsCatalogObjectGet(const char *path)
{
    CatalogPtr catalog = Catalog::getInstance();
    ObjectPtr object = catalog->getObject(path);
    return object.get();
}

/**
 * @brief ngsCatalogObjectType Returns input object handler type
 * @param object Object handler
 * @return Object type - the value from ngsCatalogObjectType
 */
enum ngsCatalogObjectType ngsCatalogObjectType(CatalogObjectH object)
{
    if(nullptr == object)
        return ngsCatalogObjectType::CAT_UNKNOWN;
    return static_cast<Object*>(object)->getType();
}

//------------------------------------------------------------------------------
// Map
//------------------------------------------------------------------------------

/**
 * @brief ngsMapCreate Creates new empty map
 * @param name Map name
 * @param description Map description
 * @param epsg EPSG code
 * @param minX minimum X coordinate
 * @param minY minimum Y coordinate
 * @param maxX maximum X coordinate
 * @param maxY maximum Y coordinate
 * @return 0 if create failed or map id.
 */
unsigned char ngsMapCreate(const char* name, const char* description,
                 unsigned short epsg, double minX, double minY,
                 double maxX, double maxY)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore)
        return MapStore::invalidMapId();
    Envelope bound(minX, minY, maxX, maxY);
    return mapStore->createMap(name, description, epsg, bound);
}

/**
 * @brief ngsMapOpen Opens existing map from file
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
 * @brief ngsMapSave Saves map to file
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
        return ngsCode::COD_SAVE_FAILED;
    }

    return ngsCode::COD_SUCCESS;
}

/**
 * @brief ngsMapClose Closes map and free resources
 * @param mapId Map id to close
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapClose(unsigned char mapId)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore)
        return errorMessage(ngsCode::COD_CLOSE_FAILED,
                            _("MapStore is not initialized"));
    return mapStore->closeMap(mapId) ? ngsCode::COD_SUCCESS :
                                       ngsCode::COD_CLOSE_FAILED;
}


/**
 * @brief ngsMapSetSize Sets map size in pixels
 * @param mapId Map id received from create or open map functions
 * @param width Output image width
 * @param height Output image height
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapSetSize(unsigned char mapId, int width, int height, int isYAxisInverted)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore)
        return errorMessage(ngsCode::COD_CLOSE_FAILED,
                            _("MapStore is not initialized"));
    return mapStore->setMapSize(mapId, width, height, isYAxisInverted == 1 ?
                                    true : false) ? ngsCode::COD_SUCCESS :
                                                    ngsCode::COD_SET_FAILED;
}

/**
 * @brief ngsDrawMap Starts drawing map in specified (in ngsInitMap) extent
 * @param mapId Map id received from create or open map functions
 * @param state Draw state (NORMAL, PRESERVED, REDRAW)
 * @param callback Progress function
 * @param callbackData Progress function arguments
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapDraw(unsigned char mapId, enum ngsDrawState state,
               ngsProgressFunc callback, void* callbackData)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore)
        return errorMessage(ngsCode::COD_DRAW_FAILED,
                            _("MapStore is not initialized"));
    Progress progress(callback, callbackData);
    return mapStore->drawMap(mapId, state, progress) ? ngsCode::COD_SUCCESS :
                                                       ngsCode::COD_DRAW_FAILED;
}

/**
 * @brief ngsGetMapBackgroundColor Map background color
 * @param mapId Map id received from create or open map functions
 * @return map background color struct
 */
ngsRGBA ngsMapGetBackgroundColor(unsigned char mapId)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_GET_FAILED,
                            _("MapStore is not initialized"));
        return {0,0,0,0};
    }
    return mapStore->getMapBackgroundColor(mapId);
}

/**
 * @brief ngsSetMapBackgroundColor Sets map background color
 * @param mapId Map id received from create or open map functions
 * @param color Background color
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapSetBackgroundColor(unsigned char mapId, const ngsRGBA &color)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        return errorMessage(ngsCode::COD_SET_FAILED,
                            _("MapStore is not initialized"));
    }
    return mapStore->setMapBackgroundColor(mapId, color) ?
                ngsCode::COD_SUCCESS : ngsCode::COD_SET_FAILED;
}

/**
 * @brief ngsMapSetCenter Sets new map center coordinates
 * @param mapId Map id
 * @param x X coordinate
 * @param y Y coordinate
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapSetCenter(unsigned char mapId, double x, double y)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        return errorMessage(ngsCode::COD_SET_FAILED,
                            _("MapStore is not initialized"));
    }
    return mapStore->setMapCenter(mapId, x, y) ?
                ngsCode::COD_SUCCESS : ngsCode::COD_SET_FAILED;
}

/**
 * @brief ngsMapGetCenter Gets map center for current view (extent)
 * @param mapId Map id
 * @return Coordintate structure. If error occured all coordinates set to 0.0
 */
ngsCoordinate ngsMapGetCenter(unsigned char mapId)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_GET_FAILED,
                            _("MapStore is not initialized"));
        return {0, 0, 0};
    }
    return mapStore->getMapCenter(mapId);
}

/**
 * @brief ngsMapGetCoordinate Georpaphic coordinates for display positon
 * @param mapId Map id
 * @param x X position
 * @param y Y position
 * @return Georpaphic coordinates
 */
ngsCoordinate ngsMapGetCoordinate(unsigned char mapId, double x, double y)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_GET_FAILED,
                            _("MapStore is not initialized"));
        return {0, 0, 0};
    }
    return mapStore->getMapCoordinate(mapId, x, y);
}

/**
 * @brief ngsMapSetScale Set current map scale
 * @param mapId Map id
 * @param scale value to set
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapSetScale(unsigned char mapId, double scale)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        return errorMessage(ngsCode::COD_SET_FAILED,
                            _("MapStore is not initialized"));
    }
    return mapStore->setMapScale(mapId, scale);
}

/**
 * @brief ngsMapGetScale Return current map scale
 * @param mapId Map id
 * @return Current map scale or 1
 */
double ngsMapGetScale(unsigned char mapId)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_GET_FAILED,
                            _("MapStore is not initialized"));
        return 1.0;
    }
    return mapStore->getMapScale(mapId);
}

/**
 * @brief ngsMapCreateLayer Creates new layer in map
 * @param mapId Map id
 * @param name Layer name
 * @param path Path to map file inside catalog in form ngc://some path/
 * @return Layer Id or -1
 */
int ngsMapCreateLayer(unsigned char mapId, const char *name, const char *path)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_CREATE_FAILED,
                     _("MapStore is not initialized"));
        return NOT_FOUND;
    }

    CatalogPtr catalog = Catalog::getInstance();
    ObjectPtr object = catalog->getObject(path);
    if(!object) {
        errorMessage(ngsCode::COD_INVALID,
                            _("Source dataset '%s' not found"), path);
        return NOT_FOUND;
    }

    return mapStore->createLayer(mapId, name, object);
}

/**
 * @brief ngsMapLayerReorder Reorder layers in map
 * @param mapId Map id
 * @param beforeLayer Before this layer insert movedLayer. May be null. In that
 * case layer will be moved to the end of map
 * @param movedLayer Layer to move
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapLayerReorder(unsigned char mapId, LayerH beforeLayer, LayerH movedLayer)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        return errorMessage(ngsCode::COD_INVALID,
                     _("MapStore is not initialized"));
    }
    return mapStore->reorderLayers(mapId, static_cast<Layer*>(beforeLayer),
                                   static_cast<Layer*>(movedLayer)) ?
                ngsCode::COD_SUCCESS : ngsCode::COD_MOVE_FAILED;
}



/**
 * @brief ngsMapSetRotate Set map rotate
 * @param mapId Map id
 * @param dir Rotate direction. May be X, Y or Z
 * @param rotate value to set
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapSetRotate(unsigned char mapId, ngsDirection dir, double rotate)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        return errorMessage(ngsCode::COD_INVALID,
                     _("MapStore is not initialized"));
    }
    return mapStore->setMapRotate(mapId, dir, rotate);
}

/**
 * @brief ngsMapGetRotate Return map rotate value
 * @param mapId Map id
 * @param dir Rotate direction. May be X, Y or Z
 * @return rotate value or 0 if error occured
 */
double ngsMapGetRotate(unsigned char mapId, ngsDirection dir)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_GET_FAILED,
                            _("MapStore is not initialized"));
        return 0.0;
    }
    return mapStore->getMapRotate(mapId, dir);
}

/**
 * @brief ngsMapGetDistance Map distance from display length
 * @param mapId Map id
 * @param w Width
 * @param h Height
 * @return ngsCoordinate where X distance along x axis and Y along y axis
 */
ngsCoordinate ngsMapGetDistance(unsigned char mapId, double w, double h)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_GET_FAILED,
                            _("MapStore is not initialized"));
        return {0.0, 0.0, 0.0};
    }
    return mapStore->getMapDistance(mapId, w, h);
}

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
 * @brief ngsMapLayerCount Returns layer count in map
 * @param mapId Map id
 * @return Layer count in map
 */
int ngsMapLayerCount(unsigned char mapId)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_GET_FAILED,
                     _("MapStore is not initialized"));
        return 0;
    }
    return static_cast<int>(mapStore->getLayerCount(mapId));
}

/**
 * @brief ngsMapLayerGet Returns map layer handle
 * @param mapId Map id
 * @param layerId Layer index
 * @return Layer handle. The caller should not delete it.
 */
LayerH ngsMapLayerGet(unsigned char mapId, int layerId)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        errorMessage(ngsCode::COD_GET_FAILED,
                     _("MapStore is not initialized"));
        return nullptr;
    }
    return mapStore->getLayer(mapId, layerId).get();
}

/**
 * @brief ngsMapLayerDelete Deletes layer from map
 * @param mapId Map id
 * @param layer Layer handler get from ngsMapLayerGet() function.
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsMapLayerDelete(unsigned char mapId, LayerH layer)
{
    MapStore* const mapStore = MapStore::getInstance();
    if(nullptr == mapStore) {
        return errorMessage(ngsCode::COD_DELETE_FAILED,
                     _("MapStore is not initialized"));
    }
    return mapStore->deleteLayer(mapId, static_cast<Layer*>(layer)) ?
                ngsCode::COD_SUCCESS : ngsCode::COD_DELETE_FAILED;
}

//------------------------------------------------------------------------------
// Layer
//------------------------------------------------------------------------------

/**
 * @brief ngsLayerGetName Returns layer name
 * @param layer Layer handler
 * @return Layer name
 */
const char *ngsLayerGetName(LayerH layer)
{
    if(nullptr == layer) {
        errorMessage(ngsCode::COD_GET_FAILED, _("Layer pointer is null"));
            return "";
    }
    return (static_cast<Layer*>(layer))->getName();
}

/**
 * @brief ngsLayerSetName Sets new layer name
 * @param layer Layer handler
 * @param name New name
 * @return ngsErrorCodes value - EC_SUCCESS if everything is OK
 */
int ngsLayerSetName(LayerH layer, const char *name)
{
    if(nullptr == layer) {
        return errorMessage(ngsCode::COD_SET_FAILED,
                            _("Layer pointer is null"));
    }
    (static_cast<Layer*>(layer))->setName(name);
    return ngsCode::COD_SUCCESS;
}
