/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
#include "ngstore/api.h"

// stl
#include <iostream>
#include <cstring>

// gdal
#include "cpl_json.h"
#include "ogr_srs_api.h"

#if __APPLE__
    #include "TargetConditionals.h"
#endif

#include "catalog/catalog.h"
#include "catalog/ngw.h"
#include "catalog/mapfile.h"
#include "catalog/folder.h"
#include "catalog/factories/connectionfactory.h"
#include "ds/simpledataset.h"
#include "ds/storefeatureclass.h"
#include "ds/util.h"
#include "map/mapstore.h"
#include "ngstore/catalog/filter.h"
#include "ngstore/version.h"
#include "ngstore/util/constants.h"
#include "util/account.h"
#include "util/authstore.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/settings.h"
#include "util/stringutil.h"
#include "util/url.h"
#include "util/versionutil.h"

using namespace ngs;

static bool gDebugMode = false;
constexpr char API_TRUE = 1;
constexpr char API_FALSE = 0;


bool isDebugMode()
{
    return gDebugMode;
}

static void initGDAL(const char *dataPath, const char *cachePath)
{
    Settings &settings = Settings::instance();
    // set config options
    if(dataPath) {
        CPLSetConfigOption("GDAL_DATA", dataPath);
        CPLDebug("ngstore", "GDAL_DATA path set to %s", dataPath);
    }

    if(cachePath) {
        CPLSetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", cachePath);
        CPLSetConfigOption("CPL_TMPDIR", cachePath);
        settings.set("common/cache_path", std::string(cachePath));

        // https://www.sqlite.org/c3ref/temp_directory.html
        // https://www.sqlite.org/tempfiles.html
#ifdef WIN32
		_putenv(CPLSPrintf("SQLITE_TMPDIR=%s", cachePath));
#else		
        setenv("SQLITE_TMPDIR", cachePath, 1);
#endif
    }

    CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS",
                       settings.getString("gdal/CPL_VSIL_ZIP_ALLOWED_EXTENSIONS",
                                          ".ngmd").c_str());

    // Generate salt for encrypt/decrypt
    std::string iv = settings.getString("crypt/iv", "");
    if(iv.empty()) {
        settings.set("crypt/iv", crypt_salt());
    }

    // Register drivers
#ifdef NGS_MOBILE // NOTE: Keep in sync with extlib.cmake gdal options. For mobile devices
    GDALRegister_VRT();
    GDALRegister_GTiff();
    GDALRegister_HFA();
    GDALRegister_PNG();
    GDALRegister_JPEG();
    GDALRegister_MEM();
    GDALRegister_WMS();
    RegisterOGRShape();
    RegisterOGRTAB();
    RegisterOGRVRT();
    RegisterOGRMEM();
    RegisterOGRGPX();
    RegisterOGRKML();
    RegisterOGRGeoJSON();
    RegisterOGRGeoPackage();
    RegisterOGRSQLite();
    RegisterOGRNGW();
#else
    GDALAllRegister();
#endif
}

static Mutex gMutex;
static std::vector<void*> cStrings;
static const char *storeCString(const std::string &str)
{
    char *data = CPLStrdup(str.c_str());
    MutexHolder holder(gMutex);
    cStrings.push_back(data);
    return data;
}

#ifdef NGS_MOBILE
constexpr size_t STRINGS_ARRAY_MAX = 150;
constexpr size_t STRINGS_ARRAY_FREE = 75;
#else
constexpr size_t STRINGS_ARRAY_MAX = 65535;
constexpr size_t STRINGS_ARRAY_FREE = 150;
#endif // NGS_MOBILE

static void clearCStrings()
{
    if(cStrings.size() > STRINGS_ARRAY_MAX) {
        MutexHolder holder(gMutex);
        for(size_t i = 0; i < STRINGS_ARRAY_FREE; ++i) {
            CPLFree(cStrings[i]);
        }
        cStrings.erase(cStrings.begin(), cStrings.begin() + STRINGS_ARRAY_FREE);
    }
}

/**
 * @brief ngsGetVersion Get library version number as major * 10000 + minor * 100 + rev
 * @param request may be gdal, proj, geos, curl, jpeg, png, zlib, iconv, sqlite3,
 *        openssl, expat, jsonc, tiff, geotiff
 * @return library version number
 */
int ngsGetVersion(const char *request)
{
    return getVersion(fromCString(request));
}

/**
 * @brief ngsGetVersionString Get library version string
 * @param request may be gdal, proj, geos, curl, jpeg, png, zlib, iconv, sqlite3,
 *        openssl, expat, jsonc, tiff, geotiff
 * @return library version string
 */
const char *ngsGetVersionString(const char *request)
{
    return storeCString(getVersionString(fromCString(request)));
}

/**
 * @brief ngsInit Init library structures
 * @param options Init library options list:
 * - CACHE_DIR - Path to cache files directory (mainly for TMS/WMS cache)
 * - SETTINGS_DIR - Path to settings directory
 * - GDAL_DATA - Path to GDAL data directory (may be skipped on Linux)
 * - DEBUG_MODE ["ON", "OFF"] - May be ON or OFF strings to enable/disable debug mode
 * - LOCALE ["en_US.UTF-8", "de_DE", "ja_JP", ...] - Locale for error messages, etc.
 * - NUM_THREADS - Number threads in various functions (a positive number or "ALL_CPUS")
 * - GL_MULTISAMPLE - Enable sampling if applicable
 * - SSL_CERT_FILE - Path to ssl cert file (*.pem)
 * - PROJ_DATA - Path to libproj data directory (may be skipped on Linux)
 * - HOME - Root directory for library
 * - APP_NAME - Application name for logs and check function availability
 * - CRYPT_KEY - Key to encrypt/decrypt passwords
 * - NEXTGIS_TRACKER_API - Tracker API endpoint URL
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsInit(char **options)
{
    gDebugMode = 0 != CPLFetchBool(options, "DEBUG_MODE", false);

    if(isDebugMode()) {
        CPLSetConfigOption("CPL_DEBUG", "ON");
        CPLSetConfigOption("CPL_CURL_VERBOSE", "ON");
    }

    CPLDebug("ngstore", "debug mode %s", gDebugMode ? "ON" : "OFF");
    const char *dataPath = CSLFetchNameValue(options, "GDAL_DATA");
    const char *cachePath = CSLFetchNameValue(options, "CACHE_DIR");
    const char *settingsPath = CSLFetchNameValue(options, "SETTINGS_DIR");
    if(settingsPath) {
        CPLSetConfigOption("NGS_SETTINGS_PATH", settingsPath);
        CPLDebug("ngstore", "NGS_SETTINGS_PATH path set to %s", settingsPath);
    }

    // Number threads
    CPLSetConfigOption("GDAL_NUM_THREADS", CPLSPrintf("%d", getNumberThreads()));
    const char *multisample = CSLFetchNameValue(options, "GL_MULTISAMPLE");
    if(multisample) {
        CPLSetConfigOption("GL_MULTISAMPLE", multisample);
    }

    const char *cainfo = CSLFetchNameValue(options, "SSL_CERT_FILE");
    if(cainfo) {
        CPLSetConfigOption("SSL_CERT_FILE", cainfo);
        CPLDebug("ngstore", "SSL_CERT_FILE path set to %s", cainfo);
    }

    const char *home = CSLFetchNameValue(options, "HOME");
    if(home) {
        CPLSetConfigOption("NGS_HOME", home);
        CPLDebug("ngstore", "NGS_HOME path set to %s", home);
    }

    const char *appName = CSLFetchNameValue(options, "APP_NAME");
    if(appName && !EQUAL(appName, "")) {
        CPLSetConfigOption("APP_NAME", appName);
        CPLDebug("ngstore", "APP_NAME set to %s", appName);
    }

    const char *cryptKey = CSLFetchNameValue(options, "CRYPT_KEY");
    if(cryptKey) {
        CPLSetConfigOption("CRYPT_KEY", cryptKey);
        CPLDebug("ngstore", "CRYPT_KEY set to %s", cryptKey);
    }

    const char *trackerApiEndpoint = CSLFetchNameValue(options, "NEXTGIS_TRACKER_API");
    if(trackerApiEndpoint) {
        Settings::instance().set("nextgis/track_api", trackerApiEndpoint);
        CPLDebug("ngstore", "NEXTGIS_TRACKER_API set to %s", trackerApiEndpoint);
    }
#ifdef HAVE_LIBINTL_H
    const char* locale = CSLFetchNameValue(options, "LOCALE");
    //TODO: Do we need std::setlocale(LC_ALL, locale); execution here in library or it will call from programm?
#endif // HAVE_LIBINTL_H

#ifdef NGS_MOBILE
    if(nullptr == dataPath) {
        return outMessage(COD_NOT_SPECIFIED, _("GDAL_DATA option is required"));
    }
#endif // NGS_MOBILE

    if(nullptr != cachePath) {
        if(!Folder::isExists(cachePath)) {
            Folder::mkDir(cachePath, true);
        }
    }

    initGDAL(dataPath, cachePath);

    const char *projData = CSLFetchNameValue(options, "PROJ_DATA");
    if(projData) {
        CPLDebug("ngstore", "PROJ_DATA set to %s", projData);
        char **pathsList = CSLAddString(nullptr, projData);
        OSRSetPROJSearchPaths(pathsList);
        CSLDestroy(pathsList);
    }

    Catalog::setInstance(new Catalog());
    MapStore::setInstance(new MapStore());

    return COD_SUCCESS;
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
void ngsFreeResources(char full)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr != mapStore) {
        mapStore->freeResources();
    }
    if(full) {
        CatalogPtr catalog = Catalog::instance();
        if(catalog) {
            catalog->freeResources();
        }
    }
    clearCStrings();
}

/**
 * @brief ngsGetLastErrorMessage Fetches the last error message posted with
 * returnError, CPLError, etc.
 * @return last error message or NULL if no error message present.
 */
const char *ngsGetLastErrorMessage()
{
    return storeCString(getLastError());
}

/**
 * @brief ngsAddNotifyFunction Add function triggered on some events
 * @param function Function executed on event occurred
 * @param notifyTypes The OR combination of ngsChangeCode
 */
void ngsAddNotifyFunction(ngsNotifyFunc function, int notifyTypes)
{
    Notify::instance().addNotifyReceiver(function, notifyTypes);
}

/**
 * @brief ngsRemoveNotifyFunction Remove function. No events will be occurred.
 * @param function The function to remove
 */
void ngsRemoveNotifyFunction(ngsNotifyFunc function)
{
    Notify::instance().deleteNotifyReceiver(function);
}

/**
 * @brief ngsSettingsGetString Returns library config settings
 * @param key Key to get settings value
 * @param defaultVal Default value if not currently set
 * @return Setting value
 */
const char *ngsSettingsGetString(const char *key, const char *defaultVal)
{
    return storeCString(Settings::instance().getString(fromCString(key),
                                                       fromCString(defaultVal)));
}

/**
 * @brief ngsSettingsSetString Sets library config settings
 * @param key Key to set settings value
 * @param value Value to set
 */
void ngsSettingsSetString(const char *key, const char *value)
{
    Settings::instance().set(fromCString(key), fromCString(value));
}

/**
 * ngsBackup create ZIP archive of settings directory and other directories provided in input. It is expected that each directory name is unique.
 * @param name Backup file name. If extension is not set it will append.
 * @param dstObjectContainer Directory to create archive.
 * @param objects List of CatalogObjectH handles. The last element must be 0.
 * @param callback Progress function (template is ngsProgressFunc) executed
 * periodically to report progress and cancel. If returns 1 the execution will
 * continue, 0 - cancelled. May be null.
 * @param callbackData Progress function parameter. May be null.
 * @return
 */
int ngsBackup(const char *name, CatalogObjectH dstObjectContainer, CatalogObjectH *objects,
        ngsProgressFunc callback, void *callbackData)
{
    if(name == nullptr) {
        return outMessage(COD_INVALID, _("Backup name must be set."));
    }
    if(objects == nullptr) {
        return outMessage(COD_INVALID, _("Backup paths array cannot be null."));
    }
    Object *dstObject = static_cast<Object*>(dstObjectContainer);
    ObjectContainer *dstCatalogObjectContainer = dynamic_cast<ObjectContainer*>(dstObject);
    if(!dstCatalogObjectContainer) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }

    if(dstCatalogObjectContainer->type() != CAT_CONTAINER_DIR) {
        return outMessage(COD_INVALID, _("Path must be a file system directory"));
    }

    if(!dstCatalogObjectContainer->canCreate(CAT_CONTAINER_ARCHIVE_ZIP)) {
        return outMessage(COD_CREATE_FAILED, _("Failed to create backup archive"));
    }

    Options options;
    options.add("OVERWRITE", "YES");
    ObjectPtr archive = dstCatalogObjectContainer->create(
        CAT_CONTAINER_ARCHIVE_ZIP, name, options);
    if(!archive) {
        return outMessage(COD_CREATE_FAILED, _("Failed to create backup archive"));
    }

    ObjectContainer *archiveCont = ngsDynamicCast(ObjectContainer, archive);
    if(!archiveCont) {
        return outMessage(COD_CREATE_FAILED, _("Failed to create backup archive"));
    }

    std::string settingPath = CPLGetConfigOption("NGS_SETTINGS_PATH", "");

    CatalogPtr catalog = Catalog::instance();
    std::vector<ObjectPtr> objectsArr;
    objectsArr.push_back(catalog->getObjectBySystemPath(settingPath));
    int counter = 0;
    while(objects[counter] != nullptr) {
        ObjectPtr copyObjectContainer = static_cast<Object*>(objects[counter])->pointer();
        objectsArr.push_back(copyObjectContainer);
        counter++;
    }

    Progress progress(callback, callbackData);
    progress.setTotalSteps(static_cast<unsigned char>(objectsArr.size()));
    unsigned char step = 0;
    progress.setStep(step);
    for(const auto &objectToCopy : objectsArr) {
        int code = archiveCont->paste(objectToCopy);
        if(code != COD_SUCCESS) {
            return code;
        }
        progress.setStep(++step);
    }

    return COD_SUCCESS;
}

//------------------------------------------------------------------------------
// GDAL proxy functions
//------------------------------------------------------------------------------

/**
 * @brief ngsGetCurrentDirectory Returns current working directory
 * @return Current directory path in OS
 */
const char *ngsGetCurrentDirectory()
{
    return CPLGetCurrentDir();
}

/**
 * @brief ngsListAddNameValue Add key=value pair into the list
 * @param list The list pointer or NULL. If NULL provided the new list will be created
 * @param name Key name to add
 * @param value Value to add
 * @return List with added key=value string. User must free returned
 * value via ngsDestroyList.
 */
char **ngsListAddNameValue(char **list, const char *name, const char *value)
{
    return CSLAddNameValue(list, name, value);
}

/**
 * @brief ngsListAddNameIntValue Add key=value pair into the list
 * @param list The list pointer or NULL. If NULL provided the new list will be created
 * @param name Key name to add
 * @param value Value to add
 * @return List with added key=value string. User must free returned
 * value via ngsDestroyList.
 */
char **ngsListAddNameIntValue(char **list, const char *name, int value)
{
    return CSLAddNameValue(list, name, CPLSPrintf("%d", value));
}

/**
 * @brief ngsDestroyList Destroy list created using ngsListAddNameValue
 * @param list The list to destroy.
 */
void ngsListFree(char **list)
{
    CSLDestroy(list);
}

/**
 * @brief ngsFormFileName Form new path string
 * @param path A parent path
 * @param name A file name
 * @param extension A file extension
 * @param catalog If catalog path 1 els 0
 * @return The new path string
 */
const char *ngsFormFileName(const char *path, const char *name,
                            const char *extension, char catalog)
{
	std::string out = CPLFormFilename(path, name, extension);
#ifdef _WIN32
	if (1 == catalog) {
		out = replace(out, "\\", Catalog::separator());
	}
#else
    ngsUnused(catalog);
#endif
    return storeCString(out);
}

/**
 * @brief ngsMalloc Allocate memory.
 * @param size Allocate memory size.
 * @return Pointer to allocated memory.
 */
void *ngsMalloc(POINTER_SIZE size)
{
    return CPLMalloc(static_cast<size_t>(size));
}

/**
 * @brief ngsFree Free pointer allocated using some function or malloc/realloc.
 * @param pointer A pointer to free.
 */
void ngsFree(void *pointer)
{
    CPLFree(pointer);
}

//------------------------------------------------------------------------------
// Miscellaneous functions
//------------------------------------------------------------------------------

/**
 * @brief ngsURLRequest Perform web request
 * @param type Request type (GET, POST, PUT, DELETE)
 * @param url Request URL
 * @param options Available options are:
 * - PERSISTENT=name, create persistent connection with provided name
 * - CLOSE_PERSISTENT=name, close persistent connection with provided name
 * - CONNECTTIMEOUT=val, where val is in seconds (possibly with decimals).
 *   This is the maximum delay for the connection to be established before
 *   being aborted
 * - TIMEOUT=val, where val is in seconds. This is the maximum delay for the whole
 *   request to complete before being aborted
 * - LOW_SPEED_TIME=val, where val is in seconds. This is the maximum time where the
 *   transfer speed should be below the LOW_SPEED_LIMIT (if not specified 1b/s),
 *   before the transfer to be considered too slow and aborted
 * - LOW_SPEED_LIMIT=val, where val is in bytes/second. See LOW_SPEED_TIME. Has only
 *   effect if LOW_SPEED_TIME is specified too
 * - HEADERS=val, where val is an extra header to use when getting a web page.
 *   For example "Accept: application/x-ogcwkt"
 * - HEADER_FILE=filename: filename of a text file with "key: value" headers
 * - HTTPAUTH=[BASIC/NTLM/GSSNEGOTIATE/ANY] to specify an authentication scheme to use
 * - USERPWD=userid:password to specify a user and password for authentication
 * - POSTFIELDS=val, where val is a null-terminated string to be passed to the server
 *   with a POST request
 * - PROXY=val, to make requests go through a proxy server, where val is of the
 *   form proxy.server.com:port_number
 * - PROXYUSERPWD=val, where val is of the form username:password
 * - PROXYAUTH=[BASIC/NTLM/DIGEST/ANY] to specify an proxy authentication scheme
 *   to use
 * - COOKIE=val, where val is formatted as COOKIE1=VALUE1; COOKIE2=VALUE2; ...
 * - MAX_RETRY=val, where val is the maximum number of retry attempts if a 503 or
 *   504 HTTP error occurs. Default is 0
 * - RETRY_DELAY=val, where val is the number of seconds between retry attempts.
 *   Default is 30
 * - MAX_FILE_SIZE=val, where val is a number of bytes
 * @param callback Progress function (template is ngsProgressFunc) executed
 * periodically to report progress and cancel. If returns 1 the execution will
 * continue, 0 - cancelled. May be null.
 * @param callbackData Progress function parameter. May be null.
 * @return structure of type ngsURLRequestResult
 */
ngsURLRequestResult *ngsURLRequest(enum ngsURLRequestType type, const char *url,
                                   char **options, ngsProgressFunc callback,
                                   void *callbackData)
{
    Options requestOptions(options);
    switch (type) {
    case URT_GET:
        requestOptions.add("CUSTOMREQUEST", "GET");
        break;
    case URT_POST:
        requestOptions.add("CUSTOMREQUEST", "POST");
        break;
    case URT_PUT:
        requestOptions.add("CUSTOMREQUEST", "PUT");
        break;
    case URT_DELETE:
        requestOptions.add("CUSTOMREQUEST", "DELETE");
        break;
    }

    Progress progress(callback, callbackData);
    return http::fetch(fromCString(url), progress, requestOptions);
}

/**
 * @brief uploadFile Upload file to url
 * @param path Path to file in operating system
 * @param url URL to upload file
 * @param progress Periodically executed progress function with parameters
 * @param options The key=value list of options:
 * - FORM_FILE_NAME=val, where val is upload file name.
 * - FORM_KEY_0=val...FORM_KEY_N, where val is name of form item.
 * - FORM_VALUE_0=val...FORM_VALUE_N, where val is value of the form item.
 * - FORM_ITEM_COUNT=val, where val is count of form items.
 * Available additional keys see in ngsURLRequest.
 * @param callback Progress function (template is ngsProgressFunc) executed
 * periodically to report progress and cancel. If returns 1 the execution will
 * continue, 0 - cancelled. May be null.
 * @param callbackData Progress function parameter. May be null.
 * @return ngsURLRequestResult structure. The result must be freed by caller via
 * delete
 */

ngsURLRequestResult *ngsURLUploadFile(const char *path, const char *url,
                                      char **options, ngsProgressFunc callback,
                                      void *callbackData)
{
    Options requestOptions(options);
    requestOptions.add("FORM_FILE_PATH", fromCString(path));
    Progress progress(callback, callbackData);

    return http::fetch(fromCString(url), progress, requestOptions);
}

/**
 * @brief ngsURLRequestResultFree Frees (deletes from memory) request result
 * @param result Request result to free
 */
void ngsURLRequestResultFree(ngsURLRequestResult *result)
{
    if(nullptr == result) {
        return;
    }
    CPLFree(result->data);
    CSLDestroy(result->headers);
    delete result;
}

/**
 * @brief ngsURLAuthAdd Adds HTTP Authorisation to store. When some HTTP
 * request executed it will ask store for authorisation header.
 * @param url The URL this authorisation options belongs to. All requests
 * started with this URL will add authorisation header.
 * @param options Authorisation options.
 * HTTPAUTH_TYPE - Requered. The authorisation type (i.e. bearer).
 * HTTPAUTH_CLIENT_ID - Client identifier for bearer
 * HTTPAUTH_TOKEN_SERVER - Token validate/update server
 * HTTPAUTH_ACCESS_TOKEN - Access token
 * HTTPAUTH_REFRESH_TOKEN - Refresh token
 * HTTPAUTH_EXPIRES_IN - Expires ime in sec.
 * HTTPAUTH_CONNECTION_TIMEOUT - Connection timeout to token server. Default 5.
 * HTTPAUTH_TIMEOUT - Network timeout to token server. Default 15.
 * HTTPAUTH_MAX_RETRY - Retries count to token server. Default 5.
 * HTTPAUTH_RETRY_DELAY - Delay between retries. Default 5.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsURLAuthAdd(const char *url, char **options)
{
    if(url == nullptr) {
        return COD_INVALID;
    }
    Options opt(options);
    return AuthStore::authAdd(url, opt) ? COD_SUCCESS : COD_INSERT_FAILED;
}

/**
 * @brief ngsURLAuthGet If Authorization properties changed, this
 * function helps to get them back.
 * @param url The URL to search authorization options.
 * @return Key=value list (may be empty). User must free returned
 * value via ngsDestroyList.
 */
char **ngsURLAuthGet(const char *url)
{
    Properties properties = AuthStore::authProperties(fromCString(url));
    if(properties.empty()) {
        return nullptr;
    }
    return properties.asCPLStringList().StealList();
}

/**
 * @brief ngsURLAuthDelete Removes authorization from store
 * @param url The URL to search authorization options.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsURLAuthDelete(const char *url)
{
    AuthStore::authRemove(fromCString(url));
    return COD_SUCCESS;
}

/**
 * @brief ngsMD5 Transform string to MD5 hash
 * @param value String to transform
 * @return Hex presentation of MD5 hash
 */
const char *ngsMD5(const char *value)
{
    return storeCString(md5(fromCString(value)));
}

/**
 * @brief ngsGetDeviceId Generate unique device identifier.
 * @param regenerate If true regenerate device id.
 * @return hex string.
 */
const char *ngsGetDeviceId(bool regenerate)
{
    return storeCString(deviceId(regenerate));
}

/**
 * @brief ngsGeneratePrivateKey Generate private key to encrypt/decrypt.
 * @return random hex string.
 */
const char *ngsGeneratePrivateKey()
{
    return storeCString(random(32));
}

/**
 * @brief ngsEncryptString Encrypt string.
 * @param text String to encrypt.
 * @return Encrypted string.
 */
const char *ngsEncryptString(const char *text)
{
    return storeCString(encrypt(fromCString(text)));
}

/**
 * @brief ngsDecryptString Decrypt string.
 * @param text String to decrypt.
 * @return Decrypted string.
 */
const char *ngsDecryptString(const char *text)
{
    return storeCString(decrypt(fromCString(text)));
}

/**
 * @brief ngsJsonDocumentCreate Creates new JSON document
 * @return New JSON document handle. The handle must be deallocated using
 * ngsJsonDocumentDestroy function.
 */
JsonDocumentH ngsJsonDocumentCreate()
{
    return new CPLJSONDocument;
}

/**
 * @brief ngsJsonDocumentDestroy Destroy JSON document created using
 * ngsJsonDocumentCreate
 * @param document JSON document handle.
 */
void ngsJsonDocumentFree(JsonDocumentH document)
{
    delete static_cast<CPLJSONDocument*>(document);
}

/**
 * @brief ngsJsonDocumentLoadUrl Load JSON request result parsing chunk by chunk
 * @param document The document handle created using ngsJsonDocumentCreate
 * @param url URL to load
 * @param options A list of key=value items ot NULL. The JSON_DEPTH=10 is the
 * JSON tokener option. The other options are the same of ngsURLRequest function.
 * @param callback Function executed periodically during load process. If returns
 * false the loading canceled. May be null.
 * @param callbackData The data for callback function execution. May be null.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsJsonDocumentLoadUrl(JsonDocumentH document, const char *url, char **options,
                           ngsProgressFunc callback, void *callbackData)
{
    CPLJSONDocument *doc = static_cast<CPLJSONDocument*>(document);
    if(nullptr == doc) {
        return outMessage(COD_LOAD_FAILED, _("Layer pointer is null"));
    }

    CPLStringList requestOptions(options, FALSE);
    requestOptions = http::addAuthHeaders(url, requestOptions);
    Progress progress(callback, callbackData);
    return doc->LoadUrl(fromCString(url), requestOptions, ngsGDALProgress,
                        &progress) ? COD_SUCCESS : COD_LOAD_FAILED;
}

/**
 * @brief ngsJsonDocumentRoot Gets JSON document root object.
 * @param document JSON document
 * @return JSON object handle or NULL. The handle must be deallocated using
 * ngsJsonObjectFree function.
 */
JsonObjectH ngsJsonDocumentRoot(JsonDocumentH document)
{
    auto doc = static_cast<CPLJSONDocument*>(document);
    if(nullptr == doc) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }
    return new CPLJSONObject(doc->GetRoot());
}

/**
 * @brief ngsJsonObjectFree Free allocated json object.
 * @param object Json object handle.
 */
void ngsJsonObjectFree(JsonObjectH object)
{
    if(nullptr != object) {
        delete static_cast<CPLJSONObject*>(object);
    }
}

/**
 * @brief ngsJsonObjectType Get json object type.
 * @param object Json object handle.
 * @return Json object type as integer:
 * - Unknown = 0
 * - Null = 1
 * - Object = 2
 * - Array = 3
 * - Boolean = 4
 * - String = 5
 * - Integer = 6
 * - Long = 7
 * - Double = 8
 */
int ngsJsonObjectType(JsonObjectH object)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return static_cast<int>(CPLJSONObject::Type::Null);
    }
    return static_cast<int>(static_cast<CPLJSONObject*>(object)->GetType());
}

/**
 * @brief ngsJsonObjectValid Get is json object valid or not.
 * @param object Json object handle.
 * @return 1 if json object valid.
 */
char ngsJsonObjectValid(JsonObjectH object)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return 0;
    }
    return static_cast<CPLJSONObject*>(object)->IsValid() ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsJsonObjectName Get json object name.
 * @param object Json object handle.
 * @return Name string.
 */
const char *ngsJsonObjectName(JsonObjectH object)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return "";
    }
    return storeCString(static_cast<CPLJSONObject*>(object)->GetName());
}

/**
 * @brief ngsJsonObjectChildren Get json object children.
 * @param object Json object handle.
 * @return List of json object handles or null. List must be deallocated
 * using ngsJsonObjectChildrenListFree.
 */
JsonObjectH *ngsJsonObjectChildren(JsonObjectH object)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    std::vector<CPLJSONObject> children =
            static_cast<CPLJSONObject*>(object)->GetChildren();

    JsonObjectH *out = new JsonObjectH[children.size() + 1];
    int counter = 0;
    for(const CPLJSONObject &child : children) {
        out[counter++] = new CPLJSONObject(child);
    }
    out[counter] = nullptr;
    return out;
}

/**
 * @brief ngsJsonObjectChildrenListFree Free list returned from ngsJsonObjectChildren.
 * @param list List of json object handles to free.
 */
void ngsJsonObjectChildrenListFree(JsonObjectH *list)
{
    if(nullptr == list) {
        errorMessage(_("The object handle is null"));
    }
    int counter = 0;
    CPLJSONObject *currentJsonObject;
    while((currentJsonObject = static_cast<CPLJSONObject*>(list[counter++])) !=
          nullptr) {
        delete currentJsonObject;
    }

    delete [] list;
}

/**
 * @brief ngsJsonObjectGetString Get string value from json object.
 * @param object Json object handle.
 * @param defaultValue Default string value to return.
 * @return String value.
 */
const char *ngsJsonObjectGetString(JsonObjectH object, const char *defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }

    return storeCString(static_cast<CPLJSONObject*>(object)->ToString(
                            fromCString(defaultValue)));
}

/**
 * @brief ngsJsonObjectGetDouble Get double value from json object.
 * @param object Json object handle.
 * @param defaultValue Default double value to return.
 * @return Double value.
 */
double ngsJsonObjectGetDouble(JsonObjectH object, double defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->ToDouble(defaultValue);
}

/**
 * @brief ngsJsonObjectGetInteger Get integer value from json object.
 * @param object Json object handle.
 * @param defaultValue Default integer value to return.
 * @return Integer value.
 */
int ngsJsonObjectGetInteger(JsonObjectH object, int defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->ToInteger(defaultValue);
}

/**
 * @brief ngsJsonObjectGetLong Get long value from json object.
 * @param object Json object handle.
 * @param defaultValue Default long value to return.
 * @return Long value.
 */
long ngsJsonObjectGetLong(JsonObjectH object, long defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->ToLong(defaultValue);
}

/**
 * @brief ngsJsonObjectGetBool Get boolean value from json object.
 * @param object Json object handle.
 * @param defaultValue Default boolean value to return.
 * @return Boolean value.
 */
char ngsJsonObjectGetBool(JsonObjectH object, char defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->ToBool(defaultValue) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsJsonObjectGetStringForKey Get string value for specific key.
 * @param object Json object handle.
 * @param name Key name.
 * @param defaultValue Default string value.
 * @return String value.
 */
const char *ngsJsonObjectGetStringForKey(JsonObjectH object, const char *name,
                                         const char *defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return storeCString(static_cast<CPLJSONObject*>(object)->GetString(
                          fromCString(name), fromCString(defaultValue)));
}

/**
 * @brief ngsJsonObjectGetDoubleForKey Get double value for specific key.
 * @param object Json object handle.
 * @param name Key name.
 * @param defaultValue Default double value.
 * @return Double value.
 */
double ngsJsonObjectGetDoubleForKey(JsonObjectH object, const char *name,
                                    double defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->GetDouble(fromCString(name),
                                                          defaultValue);
}

/**
 * @brief ngsJsonObjectGetIntegerForKey Get integer value for specific key.
 * @param object Json object handle.
 * @param name Key name.
 * @param defaultValue Default integer value.
 * @return Integer value.
 */
int ngsJsonObjectGetIntegerForKey(JsonObjectH object, const char *name,
                                  int defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->GetInteger(fromCString(name),
                                                           defaultValue);
}

/**
 * @brief ngsJsonObjectGetLongForKey Get long value for specific key.
 * @param object Json object handle.
 * @param name Key name.
 * @param defaultValue Default long value.
 * @return Long value.
 */
long ngsJsonObjectGetLongForKey(JsonObjectH object, const char *name,
                                long defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->GetLong(fromCString(name),
                                                        defaultValue);
}

/**
 * @brief ngsJsonObjectGetBoolForKey Get boolean value for specific key.
 * @param object Json object handle.
 * @param name Key name.
 * @param defaultValue Default boolean value.
 * @return Boolean value.
 */
char ngsJsonObjectGetBoolForKey(JsonObjectH object, const char *name,
                               char defaultValue)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->GetBool(fromCString(name),
                                                        defaultValue) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsJsonObjectSetStringForKey Set string value to key.
 * @param object Json object handle.
 * @param name Key name.
 * @param value String value.
 * @return 1 on success.
 */
char ngsJsonObjectSetStringForKey(JsonObjectH object, const char *name,
                                 const char *value)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return 0;
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), fromCString(value));
    return 1;
}

/**
 * @brief ngsJsonObjectSetDoubleForKey Set double value to key.
 * @param object Json object handle.
 * @param name Key name.
 * @param value Double value.
 * @return 1 on success.
 */
char ngsJsonObjectSetDoubleForKey(JsonObjectH object, const char *name,
                                 double value)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), value);
    return API_TRUE;
}

/**
 * @brief ngsJsonObjectSetIntegerForKey Set integer value to key.
 * @param object Json object handle.
 * @param name Key name.
 * @param value Integer value.
 * @return 1 on success.
 */
char ngsJsonObjectSetIntegerForKey(JsonObjectH object, const char* name,
                                  int value)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), value);
    return API_TRUE;
}

/**
 * @brief ngsJsonObjectSetLongForKey Set long value to key.
 * @param object Json object handle.
 * @param name Key name.
 * @param value Long value.
 * @return 1 on success.
 */
char ngsJsonObjectSetLongForKey(JsonObjectH object, const char *name, long value)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), GInt64(value));
    return API_TRUE;
}

/**
 * @brief ngsJsonObjectSetBoolForKey Set boolean for key.
 * @param object Json object handle.
 * @param name Key name.
 * @param value Boolean value.
 * @return 1 on success.
 */
char ngsJsonObjectSetBoolForKey(JsonObjectH object, const char *name, char value)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), value);
    return API_TRUE;
}

/**
 * @brief ngsJsonObjectGetArray Get json object as array for specific key.
 * @param object Json object handle.
 * @param name Key name.
 * @return Json array object handle or null.
 */
JsonObjectH ngsJsonObjectGetArray(JsonObjectH object, const char *name)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }
    return new CPLJSONArray(static_cast<CPLJSONObject*>(object)->GetArray(name));
}

/**
 * @brief ngsJsonObjectGetObject Get json object for specific key.
 * @param object Json object handle.
 * @param name Key name.
 * @return Json array object handle or null.
 */
JsonObjectH ngsJsonObjectGetObject(JsonObjectH object, const char *name)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }
    return new CPLJSONObject(
                static_cast<CPLJSONObject*>(object)->GetObj(fromCString(name)));
}

/**
 * @brief ngsJsonArraySize Get json array size.
 * @param object Json array object handle.
 * @return Size of json array.
 */
int ngsJsonArraySize(JsonObjectH object)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return 0;
    }
    return static_cast<CPLJSONArray*>(object)->Size();
}

/**
 * @brief ngsJsonArrayItem Get json array item by index.
 * @param object Json array object handle.
 * @param index Item index.
 * @return Json object handle or null.
 */
JsonObjectH ngsJsonArrayItem(JsonObjectH object, int index)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }
    return new CPLJSONObject(static_cast<CPLJSONArray*>(object)->operator[](index));
}


//------------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------------
static ngsCatalogObjectInfo checkIfSimple(const ObjectPtr &object)
{
    SingleDataset *simpleDS = nullptr;
    if(object->type() == CAT_CONTAINER_SIMPLE) {
        simpleDS = ngsDynamicCast(SingleDataset, object);
    }

    const char *name = storeCString(object->name());
    int type = object->type();
    if(simpleDS) {
        type = simpleDS->subType();
    }
    return {name, type, object.get()};
}

/**
 * @brief catalogObjectQuery Request the contents of some catalog container object.
 * @param object Catalog object. Must be container (contain some catalog objects).
 * @param objectFilter Filter output results
 * @return List of ngsCatalogObjectInfo items or nullptr. The last element of list always nullptr.
 * The list must be freed using ngsFree function.
 */
static ngsCatalogObjectInfo *catalogObjectQuery(CatalogObjectH object,
                                                const Filter &objectFilter)
{
    auto catalogObject = static_cast<Object*>(object);
    if(nullptr == catalogObject) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    auto container = dynamic_cast<ObjectContainer*>(catalogObject);
    if(nullptr == container) {
        return nullptr;
    }


    if(!container->loadChildren()) {
        errorMessage(_("Failed to load children"));
        return nullptr;
    }

    auto children = container->getChildren();
    if(children.empty()) {
        return nullptr;
    }

    clearCStrings();
    ngsCatalogObjectInfo *output = nullptr;
    size_t outputSize = 0;
    for(const auto &child : children) {
        if(objectFilter.canDisplay(child)) {
            outputSize++;
            output = static_cast<ngsCatalogObjectInfo*>(CPLRealloc(output,
                                sizeof(ngsCatalogObjectInfo) * (outputSize + 1)));

            output[outputSize - 1] = checkIfSimple(child);
        }
    }
    if(outputSize > 0) {
        output[outputSize] = {nullptr, NOT_FOUND, nullptr};
    }
    return output;
}

/**
 * @brief ngsCatalogObjectQuery Query name and type of child objects for
 * provided path and filter
 * @param object The handle of catalog object
 * @param filter Only objects correspondent to provided filter will be return
 * @return Array of ngsCatlogObjectInfo structures. Caller must free this array
 * after using with ngsFree method
 */
ngsCatalogObjectInfo *ngsCatalogObjectQuery(CatalogObjectH object, int filter)
{
    if(filter < CAT_UNKNOWN || filter >= CAT_MAX) {
        return nullptr;
    }
    // Create filter class from filter value.
    Filter objectFilter(static_cast<enum ngsCatalogObjectType>(filter));
    return catalogObjectQuery(object, objectFilter);
}

/**
 * @brief ngsCatalogObjectQueryMultiFilter Query name and type of child objects for
 * provided path and filters
 * @param object The handle of catalog object
 * @param filter Only objects correspondent to provided filters will be return.
 * User must delete filters array manually
 * @param filterCount The filters count
 * @return Array of ngsCatlogObjectInfo structures. Caller must free this array
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
 * @brief ngsCatalogObjectDelete Delete catalog object at specified path
 * @param object The handle of catalog object
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsCatalogObjectDelete(CatalogObjectH object)
{
    auto catalogObject = static_cast<Object*>(object);
    if(nullptr == catalogObject) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }

    // Check can delete
    if(catalogObject->canDestroy()) {
        return catalogObject->destroy() ? COD_SUCCESS : COD_DELETE_FAILED;
    }
    return outMessage(COD_UNSUPPORTED,
                      _("The path cannot be deleted (write protected, locked, etc.)"));
}

/**
 * @brief ngsCatalogObjectCanCreate Check if type can be created in catalog object
 * @param object Catalog object (expected container type)
 * @param type Type to check
 * @return 1 if container can create such type, else - 0.
 */
char ngsCatalogObjectCanCreate(CatalogObjectH object, enum ngsCatalogObjectType type)
{
    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        warningMessage(_("The object handle is null"));
        return 0;
    }

    ObjectContainer * container = dynamic_cast<ObjectContainer*>(catalogObject);
    if(nullptr == container) {
        warningMessage(_("The object handle is null"));
        return 0;
    }

    // If dataset - open it.
    DatasetBase *datasetBase = dynamic_cast<DatasetBase*>(container);
    if(datasetBase && !datasetBase->isOpened()) {
        datasetBase->open(DatasetBase::defaultOpenFlags);
    }

    ConnectionBase *connectionBase = dynamic_cast<ConnectionBase*>(container);
    if(connectionBase && !connectionBase->isOpened()) {
        connectionBase->open();
    }

    return container->canCreate(type) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsCatalogObjectCreate Create new catalog object
 * @param object The handle of catalog object
 * @param name The new object name
 * @param options The array of create object options. Caller must free this
 * array after function finishes. The common values are:
 * TYPE (required) - The new object type from enum ngsCatalogObjectType
 * CREATE_UNIQUE [ON, OFF] - If name already exists in container, make it unique
 * @return Child handle or 0.
 */
CatalogObjectH ngsCatalogObjectCreate(CatalogObjectH object, const char *name, char **options)
{
    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    Options createOptions(options);
    enum ngsCatalogObjectType type = static_cast<enum ngsCatalogObjectType>(
                createOptions.asInt("TYPE", CAT_UNKNOWN));
    createOptions.remove("TYPE");
    auto container = dynamic_cast<ObjectContainer*>(catalogObject);
    if(nullptr == container) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    // If dataset - open it.
    auto datasetBase = dynamic_cast<DatasetBase*>(container);
    if(datasetBase && !datasetBase->isOpened()) {
        datasetBase->open(DatasetBase::defaultOpenFlags);
    }

    ConnectionBase *connectionBase = dynamic_cast<ConnectionBase*>(container);
    if(connectionBase && !connectionBase->isOpened()) {
        connectionBase->open();
    }

    // Check can create
    if(container->canCreate(type)) {
        ObjectPtr object = container->create(type, fromCString(name), createOptions);
        return object.get();
    }

    errorMessage(_("Cannot create such object type (%d) in path: %s. Error: %s"),
        type, catalogObject->fullName().c_str(), getLastError());
    return nullptr;
}

/**
 * @brief ngsCatalogPathFromSystem Finds catalog path
 * (i.e. ngc://Local connections/tmp) correspondent system path (i.e. /home/user/tmp
 * @param path System path
 * @return Catalog path or empty string
 */
const char *ngsCatalogPathFromSystem(const char *path)
{
    CatalogPtr catalog = Catalog::instance();
    ObjectPtr object = catalog->getObjectBySystemPath(fromCString(path));
    if(object) {
        return storeCString(object->fullName());
    }
    return "";
}

/**
 * @brief ngsCatalogObjectCopy Copy or move catalog object to another location
 * @param srcObject Source catalog object
 * @param dstObjectContainer Destination catalog container
 * @param options Copy options. This is a list of key=value items.
 * The common value is MOVE=ON indicating to move object. The other values depends on
 * destination container.
 * @param callback Progress function (template is ngsProgressFunc) executed
 * periodically to report progress and cancel. If returns 1 the execution will
 * continue, 0 - cancelled. May be null.
 * @param callbackData Progress function parameter. May be null.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsCatalogObjectCopy(CatalogObjectH srcObject,
                         CatalogObjectH dstObjectContainer, char **options,
                         ngsProgressFunc callback, void *callbackData)
{
    Object *srcCatalogObject = static_cast<Object*>(srcObject);
    ObjectContainer *dstCatalogObjectContainer =
            static_cast<ObjectContainer*>(dstObjectContainer);
    if(!srcCatalogObject || !dstCatalogObjectContainer) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }

    ObjectPtr srcCatalogObjectPointer = srcCatalogObject->pointer();

    Progress progress(callback, callbackData);
    Options copyOptions(options);
    bool move = copyOptions.asBool("MOVE", false);
    copyOptions.remove("MOVE");

    if(move && !srcCatalogObjectPointer->canDestroy()) {
        return outMessage(COD_MOVE_FAILED,
                          _("Cannot move source dataset '%s'"),
                          srcCatalogObjectPointer->fullName().c_str());
    }

    // If dataset - let's open it.
    DatasetBase *datasetBase = dynamic_cast<DatasetBase*>(dstCatalogObjectContainer);
    if(datasetBase && !datasetBase->isOpened()) {
        datasetBase->open(DatasetBase::defaultOpenFlags);
    }

    if(dstCatalogObjectContainer->canPaste(srcCatalogObjectPointer->type())) {
        return dstCatalogObjectContainer->paste(srcCatalogObjectPointer, move,
                                                copyOptions, progress);
    }

    return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                        _("Destination container '%s' cannot accept source dataset '%s'"),
                        dstCatalogObjectContainer->fullName().c_str(),
                        srcCatalogObjectPointer->fullName().c_str());
}

/**
 * @brief ngsCatalogObjectRename Renames catalog object
 * @param object The handle of catalog object
 * @param newName The new object name. The name should be unique inside object
 * parent container
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsCatalogObjectRename(CatalogObjectH object, const char *newName)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }
    if(!catalogObject->canRename()) {
        return outMessage(COD_RENAME_FAILED,
                          _("Cannot rename catalog object '%s' to '%s'"),
                          catalogObject->fullName().c_str(), newName);
    }

    return catalogObject->rename(fromCString(newName)) ?
                COD_SUCCESS : COD_RENAME_FAILED;
}

/**
 * @brief ngsCatalogObjectOptions Queries catalog object options.
 * @param object The handle of a catalog object.
 * @param optionType The one of ngsOptionTypes enum values:
 * - OT_CREATE_DATASOURCE,
 * - OT_CREATE_RASTER,
 * - OT_CREATE_LAYER,
 * - OT_CREATE_LAYER_FIELD,
 * - OT_OPEN,
 * - OT_LOAD
 * @return Options description in xml form.
 */
const char *ngsCatalogObjectOptions(CatalogObjectH object, int optionType)
{
    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return "";
    }
    if(!Filter::isDatabase(catalogObject->type())) {
        errorMessage(_("The input object not a dataset. The type is %d. Options query not supported"),
                    catalogObject->type());
        return "";
    }

    Dataset * const dataset = dynamic_cast<Dataset*>(catalogObject);
    if(nullptr == dataset) {
        errorMessage(_("The input object not a dataset. Options query not supported"));
        return "";
    }
    auto enumOptionType = static_cast<enum ngsOptionType>(optionType);

    return storeCString(dataset->options(enumOptionType));
}

/**
 * @brief ngsCatalogObjectGet Get catalog object handle by path
 * @param path Path to the catalog object
 * @return handel or null
 */
CatalogObjectH ngsCatalogObjectGet(const char *path)
{
    CatalogPtr catalog = Catalog::instance();
    ObjectPtr object = catalog->getObject(fromCString(path));
    return object.get();
}

/**
 * @brief ngsCatalogObjectGetByName Get catalog object handle by parent and child name
 * @param parent Parent handle. Expected parent is ObjectContainer.
 * @param name Child name.
 * @param fullMatch If 1 check full match of child name.
 * @return Child handle or 0.
 */
CatalogObjectH ngsCatalogObjectGetByName(CatalogObjectH parent, const char *name,
                                         char fullMatch) {
    if(name == nullptr) {
        errorMessage(_("Name must be set"));
        return nullptr;
    }
    auto catalogObject = static_cast<Object*>(parent);
    if(!catalogObject) {
        errorMessage(_("Parent handle is null"));
        return nullptr;
    }

    auto container = dynamic_cast<ObjectContainer*>(catalogObject);
    if (container == nullptr) {
        errorMessage(_("Parent handle is null"));
        return nullptr;
    }

    ObjectPtr child;
    if(!container->loadChildren()) {
        errorMessage(_("Failed to load children"));
        return nullptr;
    }
    auto children = container->getChildren();
    for(const auto &checkChild : children) {
        if(fullMatch == 1) {
            if(compare(name, checkChild->name())) {
                return checkChild.get();
            }
        }
        else {
            if(startsWith(checkChild->name(), name)) {
                child = checkChild;
            }
        }
    }

    if(child) {
        return child.get();
    }
    errorMessage(_("Child not found"));
    return nullptr;
}

/**
 * @brief ngsCatalogObjectType Returns input object handle type
 * @param object Object handle
 * @return Object type - the value from ngsCatalogObjectType
 */
enum ngsCatalogObjectType ngsCatalogObjectType(CatalogObjectH object)
{
    if(nullptr == object) {
        return CAT_UNKNOWN;
    }
    return static_cast<Object*>(object)->type();
}

/**
 * @brief ngsCatalogObjectName Returns input object handle name
 * @param object Object handle
 * @return Catalog object name
 */
const char *ngsCatalogObjectName(CatalogObjectH object)
{
    if(nullptr == object) {
        return "";
    }
    return storeCString(static_cast<Object*>(object)->name());
}

/**
 * @brief ngsCatalogObjectPath Returns input object handle full name (catalog path)
 * @param object Object handle
 * @return Catalog object full name
 */
const char *ngsCatalogObjectPath(CatalogObjectH object)
{
    if(nullptr == object) {
        return "";
    }
    return storeCString(static_cast<Object*>(object)->fullName());
}

/**
 * @brief ngsCatalogObjectProperties Get catalog object metadata.
 * @param object A catalog object the metadata requested.
 * @param domain The metadata in specific domain or NULL.
 * @return The list of key=value items or NULL. The last item of list always NULL.
 * User must free returned value via ngsDestroyList.
 */
char **ngsCatalogObjectProperties(CatalogObjectH object, const char *domain)
{
    if(nullptr == object) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    auto propetiesList = static_cast<Object*>(object)->properties(
                fromCString(domain));

    return propetiesList.asCPLStringList().StealList();
}

/**
 * @brief ngsCatalogObjectProperty Get catalog object metadata item.
 * @param object A catalog object the metadata item requested.
 * @param name Property name.
 * @param defaultValue Property default value.
 * @param domain The metadata in specific domain or NULL.
 * @return The list of key=value items or NULL. The last item of list always NULL.
 * User must free returned value via ngsDestroyList.
 */
const char *ngsCatalogObjectProperty(CatalogObjectH object, const char *name,
                                     const char *defaultValue,
                                     const char *domain)
{
    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return defaultValue;
    }
    auto propertyVal = catalogObject->property(fromCString(name),
            fromCString(defaultValue), fromCString(domain));

    return storeCString(propertyVal);
}

/**
 * @brief ngsCatalogObjectSetProperty Set catalog object property.
 * @param object Catalog object handle.
 * @param name Property name.
 * @param value Property value.
 * @param domain Property domain.
 * @return ngsCode value - COD_SUCCESS if everything is OK.
 */
int ngsCatalogObjectSetProperty(CatalogObjectH object, const char *name,
                                    const char *value, const char *domain)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }
    return catalogObject->setProperty(fromCString(name), fromCString(value),
                                      fromCString(domain)) ? COD_SUCCESS :
                                                             COD_SET_FAILED;
}

/**
 * @brief ngsCatalogObjectRefresh Refreshes object container contents
 * @param object Object container to refresh
 */
void ngsCatalogObjectRefresh(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return;
    }

    ObjectContainer *container = dynamic_cast<ObjectContainer*>(catalogObject);
    if(!container) {
        errorMessage(_("The object is not container"));
        return;
    }
    container->refresh();
}

/**
 * @brief ngsCatalogCheckConnection Checks if connection is valid
 * @param type Catalog object type
 * @param options Options needed to check connection of provided type
 * @return 1 on success or 0 on fail
 */
char ngsCatalogCheckConnection(enum ngsCatalogObjectType type, char **options)
{
    Options checkOptions(options);
    return ConnectionFactory::checkRemoteConnection(type, checkOptions) ? 1 : 0;
}

//------------------------------------------------------------------------------
// Feature class
//------------------------------------------------------------------------------

static FeatureClass *getFeatureClassFromHandle(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    ObjectPtr catalogObjectPointer = catalogObject->pointer();
    if(!catalogObjectPointer) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }

    // Unexpected but may be
    if(catalogObjectPointer->type() == CAT_CONTAINER_SIMPLE) {
        auto dataset = dynamic_cast<SingleLayerDataset*>(catalogObject);
        if(nullptr == dataset) {
            errorMessage(_("Source dataset type is incompatible"));
            return nullptr;
        }
        catalogObjectPointer = dataset->internalObject();
    }


    if(!Filter::isFeatureClass(catalogObjectPointer->type())) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }

    return ngsDynamicCast(FeatureClass, catalogObjectPointer);
}

static Table *getTableFromHandle(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    ObjectPtr catalogObjectPointer = catalogObject->pointer();
    if(!catalogObjectPointer) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }

    // Unexpected but may be
    if(catalogObjectPointer->type() == CAT_CONTAINER_SIMPLE) {
        auto dataset = dynamic_cast<SingleLayerDataset*>(catalogObject);
        if(nullptr == dataset) {
            errorMessage(_("Source dataset type is incompatible"));
            return nullptr;
        }
        catalogObjectPointer = dataset->internalObject();
    }

    if(!(Filter::isTable(catalogObjectPointer->type()) ||
         Filter::isFeatureClass(catalogObjectPointer->type()))) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }

    return ngsDynamicCast(Table, catalogObjectPointer);
}

static Raster *getRasterFromHandle(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    if(!(Filter::isRaster(catalogObject->type()))) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }

    return dynamic_cast<Raster*>(catalogObject);
}

/**
 * @brief ngsCatalogObjectOpen Open connection or database.
 * @param object Object handle to open.
 * @param openOptions Key-value list of options specific to object type. For dataset OPEN_FLAGS is integer converted to string. Default value is GDAL_OF_SHARED|GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR.
 * @return 1 on success else 0.
 */
char ngsCatalogObjectOpen(CatalogObjectH object, char **openOptions)
{
    Options oo(openOptions);

    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        warningMessage(_("The object handle is null"));
        return API_FALSE;
    }

    auto datasetBase = dynamic_cast<DatasetBase*>(catalogObject);
    if(datasetBase) {
        unsigned int openFlags = static_cast<unsigned int>(oo.asInt("OPEN_FLAGS",
                            GDAL_OF_SHARED|GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR));
        return datasetBase->open(openFlags, oo) ? API_TRUE : API_FALSE;
    }

    auto connectionBase = dynamic_cast<ConnectionBase*>(catalogObject);
    if(connectionBase) {
        return connectionBase->open() ? API_TRUE : API_FALSE;
    }

    return API_FALSE;
}

/**
 * @brief ngsCatalogObjectIsOpened Check if connectio or databes is openned.
 * @param object Object handle to check.
 * @return 1 on success else 0.
 */
char ngsCatalogObjectIsOpened(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        warningMessage(_("The object handle is null"));
        return API_FALSE;
    }

    // If dataset - open it.
    DatasetBase *datasetBase = dynamic_cast<DatasetBase*>(catalogObject);
    if(datasetBase) {
        return datasetBase->isOpened() ? API_TRUE : API_FALSE;
    }

    ConnectionBase *connectionBase = dynamic_cast<ConnectionBase*>(catalogObject);
    if(connectionBase) {
        return connectionBase->isOpened() ? API_TRUE : API_FALSE;
    }

    return API_FALSE;
}

/**
 * @brief ngsCatalogObjectClose Close connection or database.
 * @param object Object handle to close.
 * @return 1 on success else 0.
 */
char ngsCatalogObjectClose(CatalogObjectH object)
{
    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        warningMessage(_("The object handle is null"));
        return API_FALSE;
    }

    auto datasetBase = dynamic_cast<DatasetBase*>(catalogObject);
    if(datasetBase) {
        datasetBase->close();
        return API_TRUE;
    }

    auto connectionBase = dynamic_cast<ConnectionBase*>(catalogObject);
    if(connectionBase) {
        connectionBase->close();
        return API_TRUE;
    }

    auto storeObject = dynamic_cast<StoreObject*>(catalogObject);
    if(storeObject) {
        storeObject->close();
        return API_TRUE;
    }

    return API_FALSE;
}

/**
 * @brief ngsCatalogObjectSync Flush pending changes to disk and/or sync local and remote changes.
 * @param object Handle to catalog object.
 * @return 1 on success else 0.
 */
char ngsCatalogObjectSync(CatalogObjectH object)
{
    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    return catalogObject->sync() ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsFeatureClassFields Feature class fields
 * @param object Feature class handle
 * @return NULL or list of ngsField structure pointers. The list last element
 * always has nullptr name and alias, The list must be freed using ngsFree
 * function.
 */
ngsField *ngsFeatureClassFields(CatalogObjectH object)
{
    Table *table = getTableFromHandle(object);
    if(!table) {
        return nullptr;
    }

    clearCStrings();

    const std::vector<Field> &fields = table->fields();
    ngsField *fieldsList = static_cast<ngsField*>(
                CPLMalloc((fields.size() + 1) * sizeof(ngsField)));

    int count = 0;
    for(const Field &field : fields) {
        fieldsList[count++] = {storeCString(field.m_originalName),    //name
                               storeCString(field.m_alias),           //alias
                               field.m_type};

    }

    fieldsList[count] = {nullptr, nullptr, 0};
    return fieldsList;
}

/**
 * @brief ngsFeatureClassGeometryType Feature class geometry type
 * @param object Feature class handle
 * @return 0 or geometry type enum value
 *  wkbPoint = 1,  wkbLineString = 2,  wkbPolygon = 3,  wkbMultiPoint = 4,
 *  wkbMultiLineString = 5, wkbMultiPolygon = 6, wkbGeometryCollection = 7
 */
ngsGeometryType ngsFeatureClassGeometryType(CatalogObjectH object)
{
    FeatureClass* featureClass = getFeatureClassFromHandle(object);
    if(!featureClass) {
        return wkbUnknown;
    }
    return featureClass->geometryType();
}

/**
 * @brief ngsFeatureClassCreateOverviews Creates Gl optimized vector tiles
 * @param object Catalog object handle. Must be feature class or simple datasource.
 * @param options The options key-value array specific to operation.
 * @param callback Progress function (template is ngsProgressFunc) executed
 * periodically to report progress and cancel. If returns 1 the execution will
 * continue, 0 - cancelled. May be null.
 * @param callbackData Progress function parameter. May be null.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsFeatureClassCreateOverviews(CatalogObjectH object, char **options,
                                   ngsProgressFunc callback, void *callbackData)
{
    FeatureClassOverview *featureClass = dynamic_cast<FeatureClassOverview*>(
                getFeatureClassFromHandle(object));
    if(!featureClass) {
        return COD_INVALID;
    }

    Options createOptions(options);
    Progress createProgress(callback, callbackData);
    return featureClass->createOverviews(createProgress, createOptions) ?
                COD_SUCCESS : COD_CREATE_FAILED;
}


void ngsFeatureClassBatchMode(CatalogObjectH object, char enable)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return;
    }

    Dataset *dataset = dynamic_cast<Dataset*>(catalogObject);
    if(!dataset) {
        Table *table = getTableFromHandle(object);
        if(!table) {
            errorMessage(_("Source dataset type is incompatible"));
            return;
        }

        dataset = dynamic_cast<Dataset*>(table->parent());
        if(!dataset) {
            errorMessage(_("Source dataset type is incompatible"));
            return;
        }
    }

    if(!dataset->isOpened()) {
        dataset->open(DatasetBase::defaultOpenFlags);
    }

    if(enable == 1) {
        dataset->startBatchOperation();
    }
    else {
        dataset->stopBatchOperation();
    }
}

/**
 * @brief ngsFeatureClassCreateFeature Creates new feature
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @return Feature handle or NULL
 */
FeatureH ngsFeatureClassCreateFeature(CatalogObjectH object)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return nullptr;
    }

    FeaturePtr feature = table->createFeature();
    if(feature) {
        return new FeaturePtr(feature);
    }
    return nullptr;
}

/**
 * @brief ngsFeatureClassInsertFeature Inserts feature/row into the table
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @param feature Feature to insert
 * @param logEdits If log edit operation enabled this key can manage to log or
 * not this edit operation
 * @return COD_SUCCESS if everything is OK
 */
int ngsFeatureClassInsertFeature(CatalogObjectH object, FeatureH feature,
                                 char logEdits)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return COD_INVALID;
    }

    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    return table->insertFeature(*featurePtrPointer, logEdits == 1) ?
                COD_SUCCESS : COD_INSERT_FAILED;
}

/**
 * @brief ngsFeatureClassUpdateFeature Update feature/row
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @param feature Feature to update
 * @param logEdits If log edit operation enabled this key can manage to log or
 * not this edit operation
 * @return COD_SUCCESS if everything is OK
 */
int ngsFeatureClassUpdateFeature(CatalogObjectH object, FeatureH feature,
                                 char logEdits)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return COD_INVALID;
    }

    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    return table->updateFeature(*featurePtrPointer, logEdits == 1) ?
                COD_SUCCESS : COD_UPDATE_FAILED;
}

/**
 * @brief ngsFeatureClassDeleteFeature Delete feature/row from the table
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @param id Feature identifier to delete
 * @param logEdits If log edit operation enabled this key can manage to log or
 * not this edit operation
 * @return COD_SUCCESS if everything is OK
 */
int ngsFeatureClassDeleteFeature(CatalogObjectH object, POINTER_SIZE id,
                                 char logEdits)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return COD_INVALID;
    }
    return table->deleteFeature(id, logEdits == 1) ?
                COD_SUCCESS : COD_DELETE_FAILED;
}

/**
 * @brief ngsFeatureClassDeleteFeatures Delete all features/rows from the table
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @param logEdits If log edit operation enabled this key can manage to log or
 * not this edit operation
 * @return COD_SUCCESS if everything is OK
 */
int ngsFeatureClassDeleteFeatures(CatalogObjectH object, char logEdits)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return COD_INVALID;
    }
    return table->deleteFeatures(logEdits == 1) ? COD_SUCCESS : COD_DELETE_FAILED;
}

/**
 * @brief ngsFeatureClassCount Returns feature/row count
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @return Feature/Row count
 */
POINTER_SIZE ngsFeatureClassCount(CatalogObjectH object)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return 0;
    }
    return table->featureCount();
}

/**
 * @brief ngsFeatureClassResetReading Move read cursor to the begin
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 */
void ngsFeatureClassResetReading(CatalogObjectH object)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return;
    }
    table->reset();
}

/**
 * @brief ngsFeatureClassNextFeature Returns next feature/row
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @return Feature handle
 */
FeatureH ngsFeatureClassNextFeature(CatalogObjectH object)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return nullptr;
    }

    FeaturePtr out = table->nextFeature();
    if(out) {
        return new FeaturePtr(out);
    }
    return nullptr;
}

/**
 * @brief ngsFeatureClassGetFeature Returns feature by identifier
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @param id Feature identifier
 * @return Feature handle
 */
FeatureH ngsFeatureClassGetFeature(CatalogObjectH object, POINTER_SIZE id)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return nullptr;
    }
    FeaturePtr out = table->getFeature(id);
    if(out) {
        return new FeaturePtr(out);
    }
    return nullptr;
}

/**
 * @brief ngsFeatureClassSetFilter Set filter to returns features/rows
 * @param object Handle to Table, FeatureClass or SingleLayerDataset catalog object
 * @param geometryFilter Geometry handle to filter data
 * @param attributeFilter Attribute filter (the where clause)
 * @return COD_SUCCESS if everything is OK
 */
int ngsFeatureClassSetFilter(CatalogObjectH object, GeometryH geometryFilter,
                             const char *attributeFilter)
{
    FeatureClass *featureClass = getFeatureClassFromHandle(object);
    if(nullptr == featureClass) {
        return COD_INVALID;
    }
    featureClass->setAttributeFilter(fromCString(attributeFilter));
    if(nullptr == geometryFilter) {
        featureClass->setSpatialFilter(GeometryPtr());
    }
    else {
        featureClass->setSpatialFilter(
                    static_cast<OGRGeometry*>(geometryFilter)->clone());
    }
    return COD_SUCCESS;
}

int ngsFeatureClassSetSpatialFilter(CatalogObjectH object, double minX,
                                    double minY, double maxX, double maxY)
{
    FeatureClass *featureClass = getFeatureClassFromHandle(object);
    if(nullptr == featureClass) {
        return COD_INVALID;
    }
    featureClass->setSpatialFilter(minX, minY, maxX, maxY);
    return COD_SUCCESS;
}

int ngsFeatureClassDeleteEditOperation(CatalogObjectH object,
                                       ngsEditOperation operation)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return COD_INVALID;
    }

    table->deleteEditOperation(operation);

    return COD_SUCCESS;
}

ngsEditOperation *ngsFeatureClassGetEditOperations(CatalogObjectH object)
{
    auto table = getTableFromHandle(object);
    if(nullptr == table) {
        return nullptr;
    }

    auto operations = table->editOperations();
    ngsEditOperation *out = static_cast<ngsEditOperation*>(
                CPLMalloc((operations.size() + 1) * sizeof(ngsEditOperation)));
    int counter = 0;
    for(const auto &op : operations) {
        out[counter++] = op;
    }
    out[counter] = {NOT_FOUND, NOT_FOUND, CC_NOP, NOT_FOUND, NOT_FOUND};
    return out;
}

void ngsFeatureFree(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(featurePtrPointer)
        delete featurePtrPointer;
}

int ngsFeatureFieldCount(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return 0;
    }
    return (*featurePtrPointer)->GetFieldCount();
}

char ngsFeatureIsFieldSet(FeatureH feature, int fieldIndex)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }
    return (*featurePtrPointer)->IsFieldSetAndNotNull(fieldIndex) ? API_TRUE : API_FALSE;

}

POINTER_SIZE ngsFeatureGetId(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return NOT_FOUND;
    }
    return (*featurePtrPointer)->GetFID();
}

GeometryH ngsFeatureGetGeometry(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }
    return (*featurePtrPointer)->GetGeometryRef()->clone();
}

int ngsFeatureGetFieldAsInteger(FeatureH feature, int field)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return 0;
    }
    return (*featurePtrPointer)->GetFieldAsInteger(field);

}

double ngsFeatureGetFieldAsDouble(FeatureH feature, int field)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return 0.0;
    }
    return (*featurePtrPointer)->GetFieldAsDouble(field);
}

const char *ngsFeatureGetFieldAsString(FeatureH feature, int field)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return "";
    }
    return storeCString((*featurePtrPointer)->GetFieldAsString(field));
}

int ngsFeatureGetFieldAsDateTime(FeatureH feature, int field, int *year,
                                 int *month, int *day, int *hour,
                                 int *minute, float *second, int *TZFlag)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }
    return (*featurePtrPointer)->GetFieldAsDateTime(field, year, month, day,
        hour, minute, second, TZFlag) == 1 ? COD_SUCCESS : COD_GET_FAILED;
}

void ngsFeatureSetGeometry(FeatureH feature, GeometryH geometry)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return;
    }

    // TODO: Reproject if differs srs

    (*featurePtrPointer)->SetGeometry(static_cast<OGRGeometry*>(geometry));
}

void ngsFeatureSetFieldInteger(FeatureH feature, int field, int value)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return;
    }
    (*featurePtrPointer)->SetField(field, value);
}

void ngsFeatureSetFieldDouble(FeatureH feature, int field, double value)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return;
    }
    (*featurePtrPointer)->SetField(field, value);
}

void ngsFeatureSetFieldString(FeatureH feature, int field, const char* value)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return;
    }
    (*featurePtrPointer)->SetField(field, value);
}

void ngsFeatureSetFieldDateTime(FeatureH feature, int field, int year, int month,
                                int day, int hour, int minute, float second,
                                int TZFlag)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return;
    }
    (*featurePtrPointer)->SetField(field, year, month, day, hour, minute, second,
                                   TZFlag);
}

GeometryH ngsFeatureCreateGeometry(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }
    OGRGeomFieldDefn *defn = (*featurePtrPointer)->GetGeomFieldDefnRef(0);
    return OGRGeometryFactory::createGeometry(defn->GetType());
}

GeometryH ngsFeatureCreateGeometryFromJson(JsonObjectH geometry)
{
    auto jsonGeometry = static_cast<CPLJSONObject*>(geometry);
    if(!jsonGeometry) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    return ngsCreateGeometryFromGeoJson(*jsonGeometry);
}

/**
 * @brief ngsGeometryFree Free geometry handle. Only useful if geometry created,
 * but not added to a feature
 * @param geometry Geometry handle
 */
void ngsGeometryFree(GeometryH geometry)
{
    auto geom = static_cast<OGRGeometry*>(geometry);
    if(geom) {
        OGRGeometryFactory::destroyGeometry(geom);
    }
}

void ngsGeometrySetPoint(GeometryH geometry, int point, double x, double y,
                         double z, double m)
{
    OGR_G_SetPointZM(geometry, point, x, y, z, m);
}

ngsExtent ngsGeometryGetEnvelope(GeometryH geometry)
{
    OGREnvelope env;
    OGR_G_GetEnvelope(geometry, &env);
    return { env.MinX, env.MinY, env.MaxX, env.MaxY };
}

int ngsGeometryTransformTo(GeometryH geometry, int EPSG)
{
    SpatialReferencePtr to = SpatialReferencePtr::importFromEPSG(EPSG);
    if(to == nullptr) {
        return outMessage(COD_UNSUPPORTED, _("Unsupported from EPSG with code %d"),
                          EPSG);
    }
    return static_cast<OGRGeometry*>(geometry)->transformTo(to.get()) == OGRERR_NONE ?
                COD_SUCCESS : COD_UPDATE_FAILED;
}

int ngsGeometryTransform(GeometryH geometry, CoordinateTransformationH ct)
{
    return static_cast<OGRGeometry*>(geometry)->transform(
                static_cast<OGRCoordinateTransformation*>(ct)) == OGRERR_NONE ?
                COD_SUCCESS : COD_UPDATE_FAILED;
}

char ngsGeometryIsEmpty(GeometryH geometry)
{
    return static_cast<OGRGeometry*>(geometry)->IsEmpty() ? API_TRUE : API_FALSE;
}

ngsGeometryType ngsGeometryGetType(GeometryH geometry)
{
    return static_cast<OGRGeometry*>(geometry)->getGeometryType();
}

const char *ngsGeometryToJson(GeometryH geometry)
{
    return storeCString(static_cast<OGRGeometry*>(geometry)->exportToJson());
}

CoordinateTransformationH ngsCoordinateTransformationCreate(int fromEPSG,
                                                            int toEPSG)
{
    if(fromEPSG == toEPSG) {
        errorMessage(_("From/To EPSG codes are equal"));
        return nullptr;
    }

    SpatialReferencePtr from = SpatialReferencePtr::importFromEPSG(fromEPSG);
    if(from == nullptr) {
        errorMessage(_("Unsupported from EPSG with code %d"), fromEPSG);
        return nullptr;
    }

    SpatialReferencePtr to = SpatialReferencePtr::importFromEPSG(toEPSG);
    if(to == nullptr) {
        errorMessage(_("Unsupported to EPSG with code %d"), toEPSG);
        return nullptr;
    }

    return OGRCreateCoordinateTransformation(from, to);
}

void ngsCoordinateTransformationFree(CoordinateTransformationH ct)
{
    OGRCoordinateTransformation::DestroyCT(
                static_cast<OGRCoordinateTransformation*>(ct));
}

ngsCoordinate ngsCoordinateTransformationDo(CoordinateTransformationH ct,
                                            ngsCoordinate coordinates)
{
    OGRCoordinateTransformation *pct = static_cast<OGRCoordinateTransformation*>(ct);
    if(!pct) {
        errorMessage(_("The object handle is null"));
        return {0.0, 0.0, 0.0};
    }

    pct->Transform(1, &coordinates.X, &coordinates.Y, &coordinates.Z);
    return coordinates;
}

POINTER_SIZE ngsFeatureAttachmentAdd(FeatureH feature, const char *name,
                                  const char *description, const char *path,
                                  char **options, char logEdits)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return NOT_FOUND;
    }

    return featurePtrPointer->addAttachment(fromCString(name),
                                            fromCString(description),
                                            fromCString(path), Options(options),
                                            logEdits == 1);
}

/**
 * @brief ngsFeatureAttachmentDeleteAll Delete all attachments from feature.
 * @param feature Feature to delete attachments from
 * @param logEditsA dd operaton to edit log or not.
 * @return 1 on success or 0.
 */
char ngsFeatureAttachmentDeleteAll(FeatureH feature, char logEdits)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    return featurePtrPointer->deleteAttachments(logEdits == 1) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsFeatureAttachmentDelete Delete attachment from feature.
 * @param feature Feature to delete attachment from.
 * @param aid Attachment identifier.
 * @param logEdits Add operaton to edit log or not.
 * @return 1 on success or 0.
 */
char ngsFeatureAttachmentDelete(FeatureH feature, POINTER_SIZE aid, char logEdits)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    return featurePtrPointer->deleteAttachment(aid, logEdits == 1) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsFeatureAttachmentsGet Get attachments of specified feature.
 * @param feature Feature to get attachments.
 * @return Array of ngsFeatureAttachmentInfo structures. User must free array using ngsFree function.
 */
ngsFeatureAttachmentInfo *ngsFeatureAttachmentsGet(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    clearCStrings();

    std::vector<FeaturePtr::AttachmentInfo> info = featurePtrPointer->attachments();
    ngsFeatureAttachmentInfo *out = static_cast<ngsFeatureAttachmentInfo*>(
                CPLMalloc((info.size() + 1) * sizeof(ngsFeatureAttachmentInfo)));
    int counter = 0;
    for(const auto &infoItem : info) {
        ngsFeatureAttachmentInfo outInfo = {infoItem.id,
                                            storeCString(infoItem.name),
                                            storeCString(infoItem.description),
                                            storeCString(infoItem.path),
                                            infoItem.size};

        out[counter++] = outInfo;
    }

    out[info.size()] = {NOT_FOUND, nullptr, nullptr, nullptr, 0};
    return out;
}

/**
 * @brief ngsFeatureAttachmentUpdate Update attachment information. To change attachment file you need to delete it and upload new one.
 * @param feature  Feature to update attachment.
 * @param aid Attachment identifier.
 * @param name New file name with extension
 * @param description New file description.
 * @param logEdits Add operaton to edit log or not.
 * @return 1 on success or 0.
 */
char ngsFeatureAttachmentUpdate(FeatureH feature, POINTER_SIZE aid,
                               const char *name, const char *description,
                               char logEdits)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    return featurePtrPointer->updateAttachment(aid, fromCString(name),
        fromCString(description), logEdits == 1) ? API_TRUE : API_FALSE;
}

//------------------------------------------------------------------------------
// Raster
//------------------------------------------------------------------------------

/**
 * @brief ngsRasterCacheArea Download tiles for zoom levels and area
 * @param object Raster to download tiles
 * @param options Key=value list of options.
 * - MINX - minimum X coordinate of bounding box
 * - MINY - minimum Y coordinate of bounding box
 * - MAXX - maximum X coordinate of bounding box
 * - MAXY - maximum Y coordinate of bounding box
 * - ZOOM_LEVELS - comma separated values of zoom levels
 * @param callback Progress function (template is ngsProgressFunc) executed
 * periodically to report progress and cancel. If returns 1 the execution will
 * continue, 0 - cancelled. May be null.
 * @param callbackData Progress function parameter. May be null.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsRasterCacheArea(CatalogObjectH object, char** options,
                       ngsProgressFunc callback, void* callbackData)
{
    Raster *raster = getRasterFromHandle(object);
    if(!raster) {
        return outMessage(COD_INVALID, _("Source dataset type is incompatible"));
    }

    Options createOptions(options);
    Progress createProgress(callback, callbackData);

    return raster->cacheArea(createOptions, createProgress) ?
                COD_SUCCESS : COD_CREATE_FAILED;
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
 * @return -1 if create failed or map identifier.
 */
char ngsMapCreate(const char *name, const char *description,
                 unsigned short epsg, double minX, double minY,
                 double maxX, double maxY)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return MapStore::invalidMapId();
    }
    Envelope bound(minX, minY, maxX, maxY);
    return mapStore->createMap(fromCString(name), fromCString(description), epsg, bound);
}

/**
 * @brief ngsMapOpen Opens existing map from file
 * @param path Path to map file inside catalog in form ngc://some path/
 * @return -1 if open failed or map id.
 */
char ngsMapOpen(const char *path)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return MapStore::invalidMapId();
    }
    CatalogPtr catalog = Catalog::instance();
    ObjectPtr object = catalog->getObject(fromCString(path));
    MapFile * const mapFile = ngsDynamicCast(MapFile, object);
    return mapStore->openMap(mapFile);
}

/**
 * @brief ngsMapSave Saves map to file
 * @param mapId Map identifier to save
 * @param path Path to store map data
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSave(char mapId, const char *path)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return COD_SAVE_FAILED;
    }
    CatalogPtr catalog = Catalog::instance();
    ObjectPtr mapFileObject = catalog->getObject(fromCString(path));
    MapFile *mapFile;
    if(mapFileObject) {
        mapFile = ngsDynamicCast(MapFile, mapFileObject);
    }
    else { // create new MapFile
        std::string newPath = File::resetExtension(fromCString(path), MapFile::extension());
        std::string saveFolder = File::getPath(newPath);
        std::string saveName = File::getFileName(newPath);
        ObjectPtr object = catalog->getObject(saveFolder);
        ObjectContainer * const container = ngsDynamicCast(ObjectContainer, object);
        mapFile = new MapFile(container, saveName, File::formFileName(object->path(), saveName, ""));
        mapFileObject = ObjectPtr(mapFile);
    }

    if(!mapStore->saveMap(mapId, mapFile)) {
        return COD_SAVE_FAILED;
    }

    return COD_SUCCESS;
}

/**
 * @brief ngsMapClose Closes map and free resources
 * @param mapId Map identifier
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapClose(char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_CLOSE_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->closeMap(mapId) ? COD_SUCCESS : COD_CLOSE_FAILED;
}

/**
 * @brief ngsMapReopen. Reopen map.
 * @param mapId Map identifier
 * @param path Path to store map data
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapReopen(char mapId, const char *path)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return COD_SAVE_FAILED;
    }
    CatalogPtr catalog = Catalog::instance();
    ObjectPtr mapFileObject = catalog->getObject(fromCString(path));
    MapFile *mapFile;
    if(mapFileObject) {
        mapFile = ngsDynamicCast(MapFile, mapFileObject);
    }
    else { // create new MapFile
        std::string newPath = File::resetExtension(fromCString(path),
                                                   MapFile::extension());
        std::string saveFolder = File::getPath(newPath);
        std::string saveName = File::getFileName(newPath);
        ObjectPtr object = catalog->getObject(saveFolder);
        ObjectContainer * const container = ngsDynamicCast(ObjectContainer, object);
        mapFile = new MapFile(container, saveName,
                              File::formFileName(object->path(), saveName, ""));
        mapFileObject = ObjectPtr(mapFile);
    }
    return mapStore->reopenMap(mapId, mapFile) ? COD_SUCCESS : COD_CLOSE_FAILED;
}


/**
 * @brief ngsMapSetSize Sets map size in pixels
 * @param mapId Map identifier received from create or open map functions
 * @param width Output image width
 * @param height Output image height
 * @param isYAxisInverted Is Y axis inverted (1 - inverted, 0 - ont inverted)
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetSize(char mapId, int width, int height, char isYAxisInverted)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_CLOSE_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->setMapSize(mapId, width, height, isYAxisInverted == 1) ? COD_SUCCESS : COD_SET_FAILED;
}

/**
 * @brief ngsDrawMap Starts drawing map in specified (in ngsInitMap) extent
 * @param mapId Map identifier received from create or open map functions
 * @param state Draw state (NORMAL, PRESERVED, REDRAW)
 * @param callback Progress function (template is ngsProgressFunc) executed
 * periodically to report progress and cancel. If returns 1 the execution will
 * continue, 0 - cancelled. May be null.
 * @param callbackData Progress function parameter. May be null.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapDraw(char mapId, enum ngsDrawState state, ngsProgressFunc callback, void *callbackData)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DRAW_FAILED, _("MapStore is not initialized"));
    }
    Progress progress(callback, callbackData);
    return mapStore->drawMap(mapId, state, progress) ? COD_SUCCESS : COD_DRAW_FAILED;
}

int ngsMapInvalidate(char mapId, ngsExtent bounds)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_UPDATE_FAILED, _("MapStore is not initialized"));
    }
    Envelope env(bounds.minX, bounds.minY, bounds.maxX, bounds.maxY);
    mapStore->invalidateMap(mapId, env);
    return COD_SUCCESS;
}

/**
 * @brief ngsGetMapBackgroundColor Map background color
 * @param mapId Map identifier received from create or open map functions
 * @return map background color struct
 */
ngsRGBA ngsMapGetBackgroundColor(char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return {0,0,0,0};
    }
    return mapStore->getMapBackgroundColor(mapId);
}

/**
 * @brief ngsSetMapBackgroundColor Sets map background color
 * @param mapId Map identifier received from create or open map functions
 * @param color Background color
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetBackgroundColor(char mapId, ngsRGBA color)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_SET_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->setMapBackgroundColor(mapId, color) ?
                COD_SUCCESS : COD_SET_FAILED;
}

/**
 * @brief ngsMapSetCenter Sets new map center coordinates
 * @param mapId Map identifier
 * @param x X coordinate
 * @param y Y coordinate
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetCenter(char mapId, double x, double y)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_SET_FAILED,  _("MapStore is not initialized"));
    }
    return mapStore->setMapCenter(mapId, x, y) ? COD_SUCCESS : COD_SET_FAILED;
}

/**
 * @brief ngsMapGetCenter Gets map center for current view (extent)
 * @param mapId Map identifier
 * @return Coordinate structure. If error occurred all coordinates set to 0.0
 */
ngsCoordinate ngsMapGetCenter(char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return {0, 0, 0};
    }
    return mapStore->getMapCenter(mapId);
}

/**
 * @brief ngsMapGetCoordinate Geographic coordinates for display position
 * @param mapId Map identifier
 * @param x X position
 * @param y Y position
 * @return Geographic coordinates
 */
ngsCoordinate ngsMapGetCoordinate(char mapId, double x, double y)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return {0, 0, 0};
    }
    return mapStore->getMapCoordinate(mapId, x, y);
}

/**
 * @brief ngsMapSetScale Sets current map scale
 * @param mapId Map identifier
 * @param scale value to set
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetScale(char mapId, double scale)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_SET_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->setMapScale(mapId, scale);
}

/**
 * @brief ngsMapGetScale Returns current map scale
 * @param mapId Map identifier
 * @return Current map scale or 1
 */
double ngsMapGetScale(char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return 1.0;
    }
    return mapStore->getMapScale(mapId);
}

/**
 * @brief ngsMapCreateLayer Creates new layer in map
 * @param mapId Map identifier
 * @param name Layer name
 * @param path Path to map file inside catalog in form ngc://some path/
 * @return Layer Id or -1
 */
int ngsMapCreateLayer(char mapId, const char *name, const char *path)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return NOT_FOUND;
    }

    CatalogPtr catalog = Catalog::instance();
    ObjectPtr object = catalog->getObject(path);
    if(!object) {
        errorMessage(_("Source dataset '%s' not found"), path);
        return NOT_FOUND;
    }

    return mapStore->createLayer(mapId, fromCString(name), object);
}

/**
 * @brief ngsMapLayerReorder Reorders layers in map
 * @param mapId Map identifier
 * @param beforeLayer Before this layer insert movedLayer. May be null. In that
 * case layer will be moved to the end of map
 * @param movedLayer Layer to move
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapLayerReorder(char mapId, LayerH beforeLayer, LayerH movedLayer)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_INVALID, _("MapStore is not initialized"));
    }
    return mapStore->reorderLayers(
                mapId, static_cast<Layer*>(beforeLayer),
                static_cast<Layer*>(movedLayer)) ? COD_SUCCESS : COD_MOVE_FAILED;
}

/**
 * @brief ngsMapSetRotate Sets map rotate
 * @param mapId Map identifier
 * @param dir Rotate direction. May be X, Y or Z
 * @param rotate value to set
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetRotate(char mapId, ngsDirection dir, double rotate)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_INVALID, _("MapStore is not initialized"));
    }
    return mapStore->setMapRotate(mapId, dir, rotate);
}

/**
 * @brief ngsMapGetRotate Returns map rotate value
 * @param mapId Map identifier
 * @param dir Rotate direction. May be X, Y or Z
 * @return rotate value or 0 if error occured
 */
double ngsMapGetRotate(char mapId, ngsDirection dir)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return 0.0;
    }
    return mapStore->getMapRotate(mapId, dir);
}

/**
 * @brief ngsMapGetDistance Map distance from display length
 * @param mapId Map identifier
 * @param w Width
 * @param h Height
 * @return ngsCoordinate where X distance along x axis and Y along y axis
 */
ngsCoordinate ngsMapGetDistance(char mapId, double w, double h)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return {0.0, 0.0, 0.0};
    }
    return mapStore->getMapDistance(mapId, w, h);
}

/**
 * @brief ngsMapLayerCount Returns layer count in map
 * @param mapId Map identifier
 * @return Layer count in map
 */
int ngsMapLayerCount(char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return 0;
    }
    return static_cast<int>(mapStore->getLayerCount(mapId));
}

/**
 * @brief ngsMapLayerGet Returns map layer handle
 * @param mapId Map identifier
 * @param layerId Layer index
 * @return Layer handle. The caller should not delete it.
 */
LayerH ngsMapLayerGet(char mapId, int layerId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return nullptr;
    }
    return mapStore->getLayer(mapId, layerId).get();
}

/**
 * @brief ngsMapLayerDelete Deletes layer from map
 * @param mapId Map identifier
 * @param layer Layer handle get from ngsMapLayerGet() function.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapLayerDelete(char mapId, LayerH layer)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DELETE_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->deleteLayer(mapId, static_cast<Layer*>(layer)) ?
                COD_SUCCESS : COD_DELETE_FAILED;
}

/**
 * @brief ngsMapSetOptions Set map options
 * @param mapId Map identifier
 * @param options Key=Value list of options. Available options are:
 *   ZOOM_INCREMENT - Add integer value to zoom level correspondent to scale. May be negative
 *   VIEWPORT_REDUCE_FACTOR - Reduce view size on provided value. Make sense to
 *     reduce number of tiles in map extent. The tiles will be more pixelate
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetOptions(char mapId, char **options)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DELETE_FAILED, _("MapStore is not initialized"));
    }
    Options mapOptions(options);
    return mapStore->setOptions(mapId, mapOptions) ? COD_SUCCESS : COD_SET_FAILED;
}

/**
 * @brief ngsMapSetExtentLimits Set limits to prevent pan out of them.
 * @param mapId Map identifier
 * @param minX Minimum X coordinate
 * @param minY Minimum Y coordinate
 * @param maxX Maximum X coordinate
 * @param maxY Maximum Y coordinate
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetExtentLimits(char mapId, double minX, double minY, double maxX, double maxY)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DELETE_FAILED, _("MapStore is not initialized"));
    }

    Envelope extentLimits(minX, minY, maxX, maxY);
    return mapStore->setExtentLimits(mapId, extentLimits) ?
                COD_SUCCESS : COD_SET_FAILED;
}

ngsExtent ngsMapGetExtent(char mapId, int epsg)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return {0.0, 0.0, 0.0, 0.0};
    }

    auto map = mapStore->getMap(mapId);
    if(map) {
        unsigned short fromEPSG = map->epsg();
        Envelope env = map->getExtent();

        if(fromEPSG == epsg) {
            return {env.minX(), env.minY(), env.maxX(), env.maxY()};
        }

        SpatialReferencePtr from = SpatialReferencePtr::importFromEPSG(fromEPSG);
        if(from == nullptr) {
            errorMessage(_("Unsupported from EPSG with code %d"), fromEPSG);
            return {0.0, 0.0, 0.0, 0.0};
        }

        SpatialReferencePtr to = SpatialReferencePtr::importFromEPSG(epsg);
        if(to == nullptr) {
            errorMessage(_("Unsupported from EPSG with code %d"), epsg);
            return {0.0, 0.0, 0.0, 0.0};
        }

        OGRCoordinateTransformation* ct =
                OGRCreateCoordinateTransformation(from, to);
        if(nullptr != ct) {
            double x[4], y[4];
            x[0] = env.minX();
            y[0] = env.minY();
            x[1] = env.minX();
            y[1] = env.maxY();
            x[2] = env.maxX();
            y[2] = env.maxY();
            x[3] = env.maxX();
            y[3] = env.minY();
            ct->Transform(4, x, y, nullptr);

            ngsExtent out = {100000000.0, 100000000.0, -100000000.0, -100000000.0};
            for(int i = 0; i < 4; ++i) {
                if(x[i] < out.minX)
                    out.minX = x[i];
                if(x[i] > out.maxX)
                    out.maxX = x[i];
                if(y[i] < out.minY)
                    out.minY = y[i];
                if(y[i] > out.maxY)
                    out.maxY = y[i];
            }

            return out;
        }
    }

    return {0.0, 0.0, 0.0, 0.0};
}

int ngsMapSetExtent(char mapId, ngsExtent extent)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_SET_FAILED, _("MapStore is not initialized"));
    }
    auto map = mapStore->getMap(mapId);
    if(map) {
        Envelope env(extent.minX, extent.minY, extent.maxX, extent.maxY);
        if(map->setExtent(env)) {
            return COD_SUCCESS;
        }
    }
    return COD_SET_FAILED;
}

/**
 * @brief ngsMapGetSelectionStyle Map selection style as json
 * @param mapId Map identifier
 * @param styleType Style type (Point, Line or fill)
 * @return NULL or JSON style handle. The handle must be freed by ngsJsonObjectFree.
 */
JsonObjectH ngsMapGetSelectionStyle(char mapId, enum ngsStyleType styleType)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return nullptr;
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        errorMessage(_("MapView pointer is null"));
        return nullptr;
    }

    return new CPLJSONObject(mapView->selectionStyle(styleType));
}

int ngsMapSetSelectionsStyle(char mapId, enum ngsStyleType styleType, JsonObjectH style)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_SET_FAILED,  _("MapStore is not initialized"));
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        return outMessage(COD_SET_FAILED,  _("Failed to get mapview"));
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(style);

    return mapView->setSelectionStyle(styleType, *gdalJsonObject) ?
                COD_SUCCESS : COD_SET_FAILED;
}

const char *ngsMapGetSelectionStyleName(char mapId, ngsStyleType styleType)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return "";
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        errorMessage(_("Failed to get mapview"));
        return "";
    }

    return storeCString(mapView->selectionStyleName(styleType));
}

int ngsMapSetSelectionStyleName(char mapId, enum ngsStyleType styleType, const char *name)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_SET_FAILED,  _("MapStore is not initialized"));
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        return outMessage(COD_SET_FAILED,  _("Failed to get mapview"));
    }
    return mapView->setSelectionStyleName(styleType, fromCString(name)) ?
                COD_SUCCESS : COD_SET_FAILED;
}

int ngsMapIconSetAdd(char mapId, const char *name, const char *path, char ownByMap)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_INSERT_FAILED, _("MapStore is not initialized"));
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        return outMessage(COD_INSERT_FAILED, _("Failed to get mapview"));
    }
    return mapView->addIconSet(fromCString(name), fromCString(path), ownByMap == 1) ?
                COD_SUCCESS : COD_INSERT_FAILED;
}

int ngsMapIconSetRemove(char mapId, const char *name)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DELETE_FAILED, _("MapStore is not initialized"));
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        return outMessage(COD_DELETE_FAILED, _("Failed to get mapview"));
    }

    return mapView->removeIconSet(fromCString(name)) ? COD_SUCCESS : COD_DELETE_FAILED;
}

char ngsMapIconSetExists(char mapId, const char *name)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return errorMessage(_("MapStore is not initialized"));
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        return errorMessage(_("Failed to get mapview"));
    }

    return mapView->hasIconSet(fromCString(name));
}

//------------------------------------------------------------------------------
// Layer
//------------------------------------------------------------------------------

/**
 * @brief ngsLayerGetName Returns layer name
 * @param layer Layer handle
 * @return Layer name
 */
const char *ngsLayerGetName(LayerH layer)
{
    if(nullptr == layer) {
        errorMessage(_("Layer pointer is null"));
        return "";
    }
    return storeCString(static_cast<Layer*>(layer)->name());
}

/**
 * @brief ngsLayerSetName Sets new layer name
 * @param layer Layer handle
 * @param name New name
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsLayerSetName(LayerH layer, const char *name)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }
    static_cast<Layer*>(layer)->setName(fromCString(name));
    return COD_SUCCESS;
}

/**
 * @brief ngsLayerGetVisible Returns layer visible state
 * @param layer Layer handle
 * @return true if visible or false
 */
char ngsLayerGetVisible(LayerH layer)
{
    if(nullptr == layer) {
        return errorMessage(_("Layer pointer is null"));
    }
    return static_cast<Layer*>(layer)->visible();
}

/**
 * @brief ngsLayerGetMaxZoom Returns layer maximum available zoom
 * @param layer Layer handle
 * @return maximum available zoom level there layer is shown
 */
float ngsLayerGetMaxZoom(LayerH layer)
{
    if(nullptr == layer) {
        return errorMessage(_("Layer pointer is null"));
    }
    return static_cast<Layer*>(layer)->maxZoom();
}

/**
 * @brief ngsLayerGetMinZoom Returns layer minimum available zoom
 * @param layer Layer handle
 * @return minimum available zoom level there layer is shown
 */
float ngsLayerGetMinZoom(LayerH layer)
{
    if(nullptr == layer) {
        return errorMessage(_("Layer pointer is null"));
    }
    return static_cast<Layer*>(layer)->minZoom();
}

/**
 * @brief ngsLayerSetVisible Sets layer visibility
 * @param layer Layer handle
 * @param visible
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsLayerSetVisible(LayerH layer, char visible)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }
    static_cast<Layer*>(layer)->setVisible(visible == 1);
    return COD_SUCCESS;
}

/**
 * @brief ngsLayerSetMaxZoom Sets layer maximum available zoom
 * @param layer Layer handle
 * @param zoom
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsLayerSetMaxZoom(LayerH layer, float zoom)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }
    static_cast<Layer*>(layer)->setMaxZoom(zoom);
    return COD_SUCCESS;
}

/**
 * @brief ngsLayerSetMinZoom Sets layer minimum available zoom
 * @param layer Layer handle
 * @param zoom
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsLayerSetMinZoom(LayerH layer, float zoom)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }
    static_cast<Layer*>(layer)->setMinZoom(zoom);
    return COD_SUCCESS;
}

/**
 * @brief ngsLayerGetDataSource Layer datasource
 * @param layer Layer handle
 * @return Layer datasource catalog object or NULL
 */
CatalogObjectH ngsLayerGetDataSource(LayerH layer)
{
    if(nullptr == layer) {
        errorMessage(_("Layer pointer is null"));
        return nullptr;
    }

    return static_cast<Layer*>(layer)->datasource().get();
}

JsonObjectH ngsLayerGetStyle(LayerH layer)
{
    if(nullptr == layer) {
        errorMessage(_("Layer pointer is null"));
        return nullptr;
    }

    Layer *layerPtr = static_cast<Layer*>(layer);
    IRenderLayer *renderLayerPtr = dynamic_cast<IRenderLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        errorMessage(_("Layer type is unsupported. Mast be GlRenderLayer"));
        return nullptr;
    }

    return new CPLJSONObject(renderLayerPtr->style());
}

int ngsLayerSetStyle(LayerH layer, JsonObjectH style)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }

    Layer *layerPtr = static_cast<Layer*>(layer);
    IRenderLayer *renderLayerPtr = dynamic_cast<IRenderLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        errorMessage(_("Layer type is unsupported. Mast be GlRenderLayer"));
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(style);
    return renderLayerPtr->setStyle(*gdalJsonObject) ? COD_SUCCESS : COD_SET_FAILED;
}

const char *ngsLayerGetStyleName(LayerH layer)
{
    if(nullptr == layer) {
        errorMessage(_("Layer pointer is null"));
        return "";
    }

    Layer *layerPtr = static_cast<Layer*>(layer);
    IRenderLayer *renderLayerPtr = dynamic_cast<IRenderLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        errorMessage(_("Layer type is unsupported. Mast be GlRenderLayer"));
        return "";
    }

    return storeCString(renderLayerPtr->styleName());
}

int ngsLayerSetStyleName(LayerH layer, const char *name)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }

    Layer *layerPtr = static_cast<Layer*>(layer);
    IRenderLayer *renderLayerPtr = dynamic_cast<IRenderLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        return outMessage(COD_UNSUPPORTED, _("Layer type is unsupported. Mast be GlRenderLayer"));
    }

    return renderLayerPtr->setStyleName(fromCString(name)) ? COD_SUCCESS : COD_SET_FAILED;
}

int ngsLayerSetSelectionIds(LayerH layer, POINTER_SIZE *ids, int size)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }
    Layer *layerPtr = static_cast<Layer*>(layer);
    ISelectableFeatureLayer *renderLayerPtr =
            dynamic_cast<ISelectableFeatureLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        return outMessage(COD_UNSUPPORTED, _("Layer type is unsupported. Mast be GlFeatureLayer"));
    }

    std::set<GIntBig> selectIds;
    for(int i = 0; i < size; ++i) {
        selectIds.insert(ids[i]);
    }
    renderLayerPtr->setSelectedIds(selectIds);
    return COD_SUCCESS;
}

int ngsLayerSetHideIds(LayerH layer, POINTER_SIZE *ids, int size)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }
    Layer *layerPtr = static_cast<Layer*>(layer);
    ISelectableFeatureLayer *renderLayerPtr = dynamic_cast<ISelectableFeatureLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        return outMessage(COD_UNSUPPORTED, _("Layer type is unsupported. Mast be ISelectableFeatureLayer"));
    }

    std::set<GIntBig> hideIds;
    for(int i = 0; i < size; ++i) {
        hideIds.insert(ids[i]);
    }
    renderLayerPtr->setHideIds(hideIds);
    return COD_SUCCESS;
}

//------------------------------------------------------------------------------
// Overlay
//------------------------------------------------------------------------------


static OverlayPtr getOverlayPtr(char mapId, enum ngsMapOverlayType type)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        errorMessage(_("MapStore is not initialized"));
        return nullptr;
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        errorMessage(_("MapView pointer is null"));
        return nullptr;
    }
    return mapView->getOverlay(type);
}

template<typename T>
static T *getOverlay(char mapId, enum ngsMapOverlayType type)
{
    OverlayPtr overlay = getOverlayPtr(mapId, type);
    if(!overlay) {
        errorMessage(_("Overlay pointer is null"));
        return nullptr;
    }
    return ngsDynamicCast(T, overlay);
}

int ngsOverlaySetVisible(char mapId, int typeMask, char visible)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DELETE_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->setOverlayVisible(mapId, typeMask, visible) ?
                COD_SUCCESS : COD_SET_FAILED;
}

char ngsOverlayGetVisible(char mapId, enum ngsMapOverlayType type)
{
    OverlayPtr overlay = getOverlayPtr(mapId, type);
    if(!overlay) {
        return false;
    }
    return overlay->visible();
}

/**
 * @brief ngsOverlaySetOptions Set overlay options for given overlay type
 * @param mapId Map identifier
 * @param type Overlay type
 * @param options Key=Value list of options.
 *  Available options for edit overlay are:
 *   CROSS - ON/OFF, show or hide a cross in the map center
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsOverlaySetOptions(char mapId, enum ngsMapOverlayType type, char **options)
{
    OverlayPtr overlay = getOverlayPtr(mapId, type);
    if(!overlay) {
        return COD_GET_FAILED;
    }
    Options editOptions(options);
    return overlay->setOptions(editOptions) ? COD_SUCCESS : COD_SET_FAILED;
}

/**
 * @brief ngsOverlayGetOptions Get overlay options.
 * @param mapId Map identifier.
 * @param type Overlay type.
 * @return Key=value list (may be empty). User must free returned
 * value via ngsDestroyList.
 */
char **ngsOverlayGetOptions(char mapId, enum ngsMapOverlayType type)
{
    OverlayPtr overlay = getOverlayPtr(mapId, type);
    if(!overlay) {
        return nullptr;
    }
    Options options = overlay->options();
    if(options.empty()) {
        return nullptr;
    }
    return options.asCPLStringList().StealList();
}

ngsPointId ngsEditOverlayTouch(char mapId, double x, double y, enum ngsMapTouchType type)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return {NOT_FOUND, 0};
    }
    return editOverlay->touch(x, y, type);
}

char ngsEditOverlayUndo(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return API_FALSE;
    }
    return editOverlay->undo() ? API_TRUE : API_FALSE;
}

char ngsEditOverlayRedo(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return API_FALSE;
    }
    return editOverlay->redo() ? API_TRUE : API_FALSE;
}

char ngsEditOverlayCanUndo(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return API_FALSE;
    }
    return editOverlay->canUndo() ? API_TRUE : API_FALSE;
}

char ngsEditOverlayCanRedo(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return API_FALSE;
    }
    return editOverlay->canRedo() ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsEditOverlaySave Saves edits in feature class
 * @param mapId Map identifier the edit overlay belongs to
 * @return Feature handle or NULL if error occurred
 */
FeatureH ngsEditOverlaySave(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return nullptr;
    }

    FeaturePtr savedFeature = editOverlay->save();
    if(!savedFeature) {
        errorMessage(_("Edit saving is failed"));
        return nullptr;
    }
    return new FeaturePtr(savedFeature);
}

int ngsEditOverlayCancel(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_INVALID;
    }
    editOverlay->cancel();
    return COD_SUCCESS;
}

int ngsEditOverlayCreateGeometryInLayer(char mapId, LayerH layer, char empty)
{
    if(!layer) {
        return outMessage(COD_CREATE_FAILED, _("Layer pointer is null"));
    }
    Layer *pLayer = static_cast<Layer*>(layer);
    FeatureClassPtr datasource =
            std::dynamic_pointer_cast<FeatureClass>(pLayer->datasource());
    if(!datasource) {
        return outMessage(COD_CREATE_FAILED, _("Layer datasource is null"));
    }

    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_CREATE_FAILED;
    }
    if(!editOverlay->createGeometry(datasource, empty == 1)) {
        return outMessage(COD_CREATE_FAILED, _("Geometry creation is failed"));
    }
    return COD_SUCCESS;
}


int ngsEditOverlayCreateGeometry(char mapId, ngsGeometryType type)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_CREATE_FAILED;
    }
    if(!editOverlay->createGeometry(OGRwkbGeometryType(type))) {
        return outMessage(COD_CREATE_FAILED, _("Geometry creation is failed"));
    }
    return COD_SUCCESS;
}

int ngsEditOverlayEditGeometry(char mapId, LayerH layer, POINTER_SIZE featureId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_UPDATE_FAILED;
    }

    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
    }

    LayerPtr editLayer;
    if(layer) {
        int layerCount = static_cast<int>(mapStore->getLayerCount(mapId));
        for(int i = 0; i < layerCount; ++i) {
            LayerPtr layerPtr = mapStore->getLayer(mapId, i);
            if(layerPtr.get() == static_cast<Layer*>(layer)) {
                editLayer = layerPtr;
                break;
            }
        }
    }
    if(!editLayer) {
        return outMessage(COD_UPDATE_FAILED, _("Geometry edit is failed"));
    }

    if(!editOverlay->editGeometry(editLayer, featureId)) {
        return outMessage(COD_UPDATE_FAILED, _("Geometry edit is failed"));
    }
    return COD_SUCCESS;
}

int ngsEditOverlayDeleteGeometry(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_DELETE_FAILED;
    }

    if(editOverlay->isGeometryValid() && !editOverlay->deleteGeometry()) {
        return outMessage(COD_DELETE_FAILED, _("Geometry delete failed"));
    }
    return COD_SUCCESS;
}

int ngsEditOverlayAddPoint(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_INSERT_FAILED;
    }

    return editOverlay->createPoint() ? COD_SUCCESS : COD_INSERT_FAILED;
}

int ngsEditOverlayAddVertex(char mapId, ngsCoordinate coordinates)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_INSERT_FAILED;
    }

    return editOverlay->addPoint(coordinates.X, coordinates.Y) ?
                COD_SUCCESS : COD_INSERT_FAILED;
}

enum ngsEditDeleteResult ngsEditOverlayDeletePoint(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return EDT_SELTYPE_NO_CHANGE;
    }
    return editOverlay->deletePoint();
}

int ngsEditOverlayAddHole(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_INSERT_FAILED;
    }
    if(!editOverlay->addHole()) {
        return outMessage(COD_INSERT_FAILED, _("Add hole failed"));
    }
    return COD_SUCCESS;
}

enum ngsEditDeleteResult ngsEditOverlayDeleteHole(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
       return EDT_SELTYPE_NO_CHANGE;
    }
    return editOverlay->deleteHole();
}

int ngsEditOverlayAddGeometryPart(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_INSERT_FAILED;
    }
    if(!editOverlay->addGeometryPart()) {
        return outMessage(COD_INSERT_FAILED, _("Geometry part adding is failed"));
    }
    return COD_SUCCESS;
}

void ngsEditOverlaySetWalkingMode(char mapId, char enable)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return;
    }
    editOverlay->setWalkingMode(enable == 1);
}

char ngsEditOverlayGetWalkingMode(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return API_FALSE;
    }
    return editOverlay->isWalkingMode() ? API_TRUE : API_FALSE;
}

/**
 *
 * @param mapId
 * @return The value from enum ngsEditDeleteResult
 */
enum ngsEditDeleteResult ngsEditOverlayDeleteGeometryPart(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return EDT_SELTYPE_NO_CHANGE;
    }
    return editOverlay->deleteGeometryPart();
}

GeometryH ngsEditOverlayGetGeometry(char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return nullptr;
    }
    return editOverlay->geometry();
}

int ngsEditOverlaySetStyle(char mapId, enum ngsEditStyleType type,
                           JsonObjectH style)
{
    EditLayerOverlay *overlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == overlay) {
        return COD_DELETE_FAILED;
    }
    return overlay->setStyle(type, *static_cast<CPLJSONObject*>(style)) ?
                COD_SUCCESS : COD_SET_FAILED;
}

int ngsEditOverlaySetStyleName(char mapId, enum ngsEditStyleType type,
                               const char* name)
{
    EditLayerOverlay *overlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == overlay) {
        return COD_DELETE_FAILED;
    }
    return overlay->setStyleName(type, name) ? COD_SUCCESS : COD_SET_FAILED;
}

JsonObjectH ngsEditOverlayGetStyle(char mapId, enum ngsEditStyleType type)
{
    EditLayerOverlay *overlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == overlay) {
        return nullptr;
    }
    return new CPLJSONObject(overlay->style(type));
}

int ngsLocationOverlayUpdate(char mapId, ngsCoordinate location, float direction,
                             float accuracy)
{
    LocationOverlay *overlay = getOverlay<LocationOverlay>(mapId, MOT_LOCATION);
    if(nullptr == overlay) {
        return COD_UPDATE_FAILED;
    }

    overlay->setLocation(location, direction, accuracy);
    return COD_SUCCESS;
}

int ngsLocationOverlaySetStyle(char mapId, JsonObjectH style)
{
    LocationOverlay *overlay = getOverlay<LocationOverlay>(mapId, MOT_LOCATION);
    if(nullptr == overlay) {
        return COD_UPDATE_FAILED;
    }

    return overlay->setStyle(*static_cast<CPLJSONObject*>(style)) ?
                COD_SUCCESS : COD_SET_FAILED;
}

int ngsLocationOverlaySetStyleName(char mapId, const char* name)
{
    LocationOverlay *overlay = getOverlay<LocationOverlay>(mapId, MOT_LOCATION);
    if(nullptr == overlay) {
        return COD_UPDATE_FAILED;
    }

    return overlay->setStyleName(name) ? COD_SUCCESS : COD_SET_FAILED;
}

JsonObjectH ngsLocationOverlayGetStyle(char mapId)
{
    LocationOverlay *overlay = getOverlay<LocationOverlay>(mapId, MOT_LOCATION);
    if(nullptr == overlay) {
        return nullptr;
    }
    return new CPLJSONObject(overlay->style());
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
 * @brief ngsQMSQuery Query QuickMapServices for specific geoservices
 * @param options key-value list. All keys are optional. Available keys are:
 *  type - services type. May be tms, wms, wfs, geojson
 *  epsg - services spatial reference EPSG code
 *  cumulative_status - services status. May be works, problematic, failed
 *  search - search string for a specific geoserver
 *  intersects_extent - only services bounding boxes intersecting provided
 *                      extents will return. Extent mast be in WKT or EWKT format.
 *  ordering - an order in which services will return. May be name, -name, id,
 *             -id, created_at, -created_at, updated_at, -updated_at
 *  limit - return services maximum count. Works together with offset.
 *  offset - offset from the beginning of the return list. Works together with limit.
 * @return array of ngsQMSItem structures. User must free array using ngsFree.
 */
ngsQMSItem *ngsQMSQuery(char **options)
{
    Options opt(options);

    ngsExtent ext = {0.0, 0.0, 0.0, 0.0};
    ngsQMSItem *out = nullptr;
    auto items = qms::QMSQuery(opt);
    if(items.empty()) {
        out = static_cast<ngsQMSItem*>(CPLMalloc(sizeof(ngsQMSItem)));
        out[0] = {NOT_FOUND, "", "", CAT_UNKNOWN, "", COD_REQUEST_FAILED, ext};
    }
    else {
        clearCStrings();
        size_t size = items.size() + 1;
        out = static_cast<ngsQMSItem*>(CPLMalloc(sizeof(ngsQMSItem) * size));
        for(size_t i = 0; i < items.size(); ++i) {
            ext = {items[i].extent.minX(), items[i].extent.minY(),
                   items[i].extent.maxX(), items[i].extent.maxY()};
            out[i] = { items[i].id,
                       storeCString(items[i].name),
                       storeCString(items[i].desc),
                       items[i].type,
                       storeCString(items[i].iconUrl),
                       items[i].status,
                       ext
                     };
        }
    }

    return out;
}

/**
 * @brief ngsQMSQueryProperties Get QuickMapServices geoservice details
 * @param itemId QuickMapServices geoservice identifier
 * @return struct of type ngsQMSItemProperties
 */
ngsQMSItemProperties ngsQMSQueryProperties(int itemId)
{
    ngsQMSItemProperties out = {NOT_FOUND, COD_REQUEST_FAILED, "", "", "", CAT_UNKNOWN,
                                NOT_FOUND, NOT_FOUND, NOT_FOUND, "", {0.0, 0.0, 0.0, 0.0}, 0};
    auto item = qms::QMSQueryProperties(itemId);
    if(item.id != NOT_FOUND) {
        clearCStrings();

        out.id = item.id;
        out.status = item.status;
        out.url = storeCString(item.url);
        out.name = storeCString(item.name);
        out.desc = storeCString(item.desc);
        out.type = item.type;
        out.EPSG = item.epsg;
        out.z_min = item.z_min;
        out.z_max = item.z_max;
        out.iconUrl = storeCString(item.iconUrl);
        out.y_origin_top = item.y_origin_top;
        out.extent = {item.extent.minX(), item.extent.minY(),
                      item.extent.maxX(), item.extent.maxY()};
    }

    return out;
}

/**
 * @brief ngsAccountGetFirstName Get NextGIS user first name.
 * @return User first name string.
 */
const char *ngsAccountGetFirstName()
{
    return storeCString(Account::instance().firstName());
}

/**
 * @brief ngsAccountGetLastName Get NextGIS user last name.
 * @return User last name string.
 */
const char *ngsAccountGetLastName()
{
    return storeCString(Account::instance().lastName());
}

/**
 * @brief ngsAccountGetEmail Get NextGIS user email.
 * @return User email string.
 */
const char *ngsAccountGetEmail()
{
    return storeCString(Account::instance().email());
}

/**
 * @brief ngsAccountBitmapPath Get avatar image path.
 * @return Avatar image path or empty string.
 */
const char *ngsAccountBitmapPath()
{
    return storeCString(Account::instance().avatarFilePath());
}

/**
 * @brief ngsAccountIsAuthorized Is user authorised or not.
 * @return 1 if user authorized.
 */
char ngsAccountIsAuthorized()
{
    return Account::instance().isUserAuthorized() ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsAccountExit Exit from account. Any persistend settings and avatar
 * image will be deleted.
 */
void ngsAccountExit()
{
    Account::instance().exit();
}

/**
 * @brief ngsAccountIsFuncAvailable Is function available.
 * @param application Application requested function.
 * @param function Function name to check.
 * @return 1 if function available.
 */
char ngsAccountIsFuncAvailable(const char *application, const char *function)
{
    return Account::instance().isFunctionAvailable(fromCString(application),
                                                   fromCString(function)) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsAccountSupported Is account supported or not.
 * @return 1 if account supported.
 */
char ngsAccountSupported()
{
    return Account::instance().isUserSupported();
}

/**
 * @brief ngsAccountUpdateUserInfo Update user information.
 * @return 1 if update finished successfully.
 */
char ngsAccountUpdateUserInfo()
{
    return Account::instance().updateUserInfo() ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsAccountUpdateSupportInfo Update user support information.
 * @return 1 if update finished successfully.
 */
char ngsAccountUpdateSupportInfo()
{
    return Account::instance().updateSupportInfo() ? API_TRUE : API_FALSE;
}

NGS_EXTERNC char ngsAccountUpdateTeamsInfo()
{
    return Account::instance().updateTeamsInfo() ? API_TRUE : API_FALSE;
}

static DataStore *getDataStoreFromHandle(CatalogObjectH object)
{
    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    return dynamic_cast<DataStore*>(catalogObject);
}

NGS_EXTERNC ngsNGWTeamInfo **ngsAccountGetTeams()
{
    const auto teams = Account::instance().teams();
    ngsNGWTeamInfo **ngwTeams = static_cast<ngsNGWTeamInfo **>(ngsMalloc(sizeof(ngsNGWTeamInfo *) * teams.size()));

    size_t i = 0;
    for (const auto &team : teams)
    {
        ngsNGWTeamInfo *ngwTeamInfo = static_cast<ngsNGWTeamInfo *>(ngsMalloc(sizeof(ngsNGWTeamInfo)));
        ngwTeamInfo->id = storeCString(team.id);
        ngwTeamInfo->ownerId = storeCString(team.ownerId);
        ngwTeamInfo->webgis = storeCString(team.webgis);
        ngwTeamInfo->startDate = storeCString(team.startDate);
        ngwTeamInfo->endDate = storeCString(team.endDate);

        ngwTeamInfo->usersSize = team.users.size();
        ngwTeamInfo->users = static_cast<ngsNGWUserInfo **>(ngsMalloc(sizeof(ngsNGWUserInfo *) * team.users.size()));

        size_t j = 0;
        for (const auto& user : team.users)
        {
            ngsNGWUserInfo *ngwUserInfo = static_cast<ngsNGWUserInfo *>(ngsMalloc(sizeof(ngsNGWUserInfo)));
            ngwUserInfo->firstName = storeCString(user.firstName);
            ngwUserInfo->lastName = storeCString(user.lastName);
            ngwUserInfo->username = storeCString(user.username);
            ngwUserInfo->guid = storeCString(user.guid);
            ngwUserInfo->locale = storeCString(user.locale);

            ngwTeamInfo->users[j++] = ngwUserInfo;
        }

        ngwTeams[i++] = ngwTeamInfo;
    }

    return ngwTeams;
}

NGS_EXTERNC size_t ngsAccountGetTeamsCount()
{
    return Account::instance().teams().size();
}

/**
 * @brief ngsStoreGetTrackTable Get tracks table from store. If table is not exists it will create.
 * @param store Store to get tracks table.
 * @return Tracks table object or NULL.
 */
CatalogObjectH ngsStoreGetTracksTable(CatalogObjectH store)
{
    auto dataStore = getDataStoreFromHandle(store);
    if(!dataStore) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }

    if(!dataStore->open(DatasetBase::defaultOpenFlags)) {
        return nullptr;
    }

    return dataStore->getTracksTable().get();
}

/**
 * @brief ngsStoreHasTracksTable Check if tracks table is exists.
 * @param store Store to check if tracks table exists.
 * @return return 1 if exists, otherwise 0.
 */
char ngsStoreHasTracksTable(CatalogObjectH store)
{
    auto dataStore = getDataStoreFromHandle(store);
    if(!dataStore) {
        errorMessage(_("Source dataset type is incompatible"));
        return API_FALSE;
    }

    if(!dataStore->isOpened()) {
        if(!dataStore->open()) {
            return API_FALSE;
        }
    }

    return dataStore->hasTracksTable() ? API_TRUE : API_FALSE;
}

static TracksTable *getTracksTableFromHandle(CatalogObjectH object)
{
    auto catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    if(!Filter::isFeatureClass(catalogObject->type())) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }

    return dynamic_cast<TracksTable*>(catalogObject);
}

/**
 * @brief ngsTrackIsRegistered Check if tracker is registered at track.nextgis.com
 * @return 1 if registered or 0 if not.
 */
char ngsTrackIsRegistered()
{
    CPLJSONDocument checkReq;
    CPLStringList headers;
    headers.AddNameValue("HEADERS", "Accept: */*");
    if(checkReq.LoadUrl(ngw::getTrackerUrl() + "/registered", headers)) {
        return checkReq.GetRoot().GetBool("registered", false) ? API_TRUE : API_FALSE;
    }
    return API_FALSE;
}

/**
 * Get internal points table. Don't add or update points in it. Use tracks table instead.
 * @param tracksTable Tracks table.
 * @return Points table connected with tracks table.
 */
CatalogObjectH ngsTrackGetPointsTable(CatalogObjectH tracksTable)
{
    auto table = getTracksTableFromHandle(tracksTable);
    if(!table) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }
    return table->getPointsLayer().get();
}

/**
 * @brief ngsTrackGetList Get tracks list.
 * @param tracksTable Table where tracks points stored.
 * @return List or NULL. User must free list using ngsFree method.
 */
ngsTrackInfo *ngsTrackGetList(CatalogObjectH tracksTable)
{
    auto table = getTracksTableFromHandle(tracksTable);
    if(!table) {
        errorMessage(_("Source dataset type is incompatible"));
        return nullptr;
    }

    auto list = table->getTracks();

    ngsTrackInfo *outList = static_cast<ngsTrackInfo*>(
            CPLMalloc((list.size() + 1) * sizeof(ngsTrackInfo)));

    int count = 0;
    for(const auto &listItem : list) {
        outList[count++] = {storeCString(listItem.name), listItem.startTimeStamp, listItem.stopTimeStamp, listItem.count};
    }

    outList[count] = {nullptr, NOT_FOUND, NOT_FOUND, 0};
    return outList;

}

/**
 * @brief ngsTrackAddPoint Add point to a tracks table.
 * @param tracksTable Table handle.
 * @param trackName Track name.
 * @param x X coordinate (EPSG:4326).
 * @param y Y coordinate (EPSG:4326).
 * @param z Z coordinate.
 * @param acc Accuracy.
 * @param speed Speed.
 * @param course Course.
 * @param timeStamp Time stamp (UTC in seconds).
 * @param satCount Satellite count using to get fix.
 * @param newTrack Is this point start new track.
 * @param newSegment Is this point start new segment.
 * @return 1 on success, 0 if failed.
 */
char ngsTrackAddPoint(CatalogObjectH tracksTable, const char *trackName, double x, double y, double z,
        float acc, float speed, float course, long timeStamp, int satCount, char newTrack, char newSegment)
{
    auto table = getTracksTableFromHandle(tracksTable);
    if(!table) {
        errorMessage(_("Source dataset type is incompatible"));
        return API_FALSE;
    }

    return table->addPoint(fromCString(trackName), x, y, z, acc, speed, course, timeStamp, satCount, newTrack,
            newSegment) ? API_TRUE : API_FALSE;
}

/**
 * ngsTrackDeletePoints Delete points from a tracks table.
 *
 * @param tracksTable Table handle.
 * @param start Start timestamp (UTC in seconds).
 * @param stop End timestamp (UTC in seconds).
 * @return 1 on success, 0 if failed.
 */
char ngsTrackDeletePoints(CatalogObjectH tracksTable, long start, long stop)
{
    auto table = getTracksTableFromHandle(tracksTable);
    if(!table) {
        errorMessage(_("Source dataset type is incompatible"));
        return API_FALSE;
    }

    table->deletePoints(start, stop);
    return EQUAL(getLastError(), "") ? API_TRUE : API_FALSE;
}

template<typename T>
static T *getObjectFromHandle(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return nullptr;
    }

    return dynamic_cast<T*>(catalogObject);
}

/**
 * @brief ngsNGWServiceDeleteLayer Delete layer from NGW service,
 * @param object Catalog object of type NGWService.
 * @param keyName Key name to delete layer.
 * @return 1 on success, 0 if failed.
 */
char ngsNGWServiceDeleteLayer(CatalogObjectH object, const char *keyName)
{
    auto service = getObjectFromHandle<NGWService>(object);
    if(nullptr == service) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWService");
        return API_FALSE;
    }
    return service->deleteLayer(fromCString(keyName)) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsNGWServiceAddLayer Add new layer to NGW WMS or WFS service.
 * @param object Catalog object of type NGWService.
 * @param keyName Key name for new layer. Mast be unique.
 * @param displayName New layer name.
 * @param ngwObject For WMS servoce the ngwObject mast be Style, for WFS - vector or PostGIS layer.
 * @return 1 on success, 0 if failed.
 */
char ngsNGWServiceAddLayer(CatalogObjectH object, const char *keyName,
                           const char *displayName, CatalogObjectH ngwObject)
{
    auto service = getObjectFromHandle<NGWService>(object);
    if(nullptr == service) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWService");
        return API_FALSE;
    }
    auto resourceBase = getObjectFromHandle<NGWResourceBase>(ngwObject);
    if(nullptr == resourceBase) {
        errorMessage(_("Cannot cast to %s from input ngwObject"), "NGWResourceBase");
        return API_FALSE;
    }

    return service->addLayer(fromCString(keyName), fromCString(displayName),
                             resourceBase) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsNGWServiceChangeLayer
 * @param object Catalog object of type NGWService.
 * @param originalKeyName Original key name
 * @param newKeyName New key name
 * @param newDisplayName New name for layer
 * @param ngwObject For WMS servoce the ngwObject mast be Style, for WFS - vector or PostGIS layer.
 * @return 1 on success, 0 if failed.
 */
char ngsNGWServiceChangeLayer(CatalogObjectH object, const char *originalKeyName,
                              const char *newKeyName, const char *newDisplayName,
                              CatalogObjectH ngwObject)
{
    auto service = getObjectFromHandle<NGWService>(object);
    if(nullptr == service) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWService");
        return API_FALSE;
    }
    NGWResourceBase *resourceBase = getObjectFromHandle<NGWResourceBase>(ngwObject);
    if(nullptr == resourceBase) {
        errorMessage(_("Cannot cast to %s from input ngwObject"), "NGWResourceBase");
        return API_FALSE;
    }

    return service->changeLayer(fromCString(originalKeyName),
                                fromCString(newKeyName),
                                fromCString(newDisplayName),
                                resourceBase) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsNGWServiceList List of service layers.
 * @param object Catalog object of type NGWService.
 * @return List of ngsNGWServiceLayerInfo items or nullptr. The last element of list always nullptr.
 * The list must be freed using ngsFree function.
 */
ngsNGWServiceLayerInfo *ngsNGWServiceList(CatalogObjectH object)
{
    auto service = getObjectFromHandle<NGWService>(object);
    if(nullptr == service) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWService");
        return nullptr;
    }

    auto layers = service->layers();
    if(layers.empty()) {
        return nullptr;
    }
    size_t outputSize = layers.size();
    ngsNGWServiceLayerInfo *output =
            static_cast<ngsNGWServiceLayerInfo*>(
                CPLMalloc(sizeof(ngsNGWServiceLayerInfo) * (outputSize + 1)));
    clearCStrings();

    for(size_t i = 0; i < outputSize; ++i) {
        const auto &layer = layers[i];
        output[i] = {storeCString(layer->m_key), storeCString(layer->m_name),
                    layer->m_resourceId};
    }

    output[outputSize] = {nullptr, nullptr, NOT_FOUND};
    return output;
}

/**
 * @brief ngsNGWWebMapDeleteBaseMap Delete base map information from web map.
 * @param object Catalog object of type NGWWebMap.
 * @param index Base map index to delete.
 * @return 1 on success, 0 if failed.
 */
char ngsNGWWebMapDeleteBaseMap(CatalogObjectH object, int index)
{
    auto webMap = getObjectFromHandle<NGWWebMap>(object);
    if(nullptr == webMap) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWWebMap");
        return API_FALSE;
    }
    return webMap->deleteBaseMap(index) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsNGWWebMapAddBaseMap Add base map to web map
 * @param object Catalog object of type NGWWebMap.
 * @param baseMap Base map information as struct.
 * @return 1 on success, 0 if failed.
 */
char ngsNGWWebMapAddBaseMap(CatalogObjectH object, ngsNGWWebmapBasemapInfo baseMap)
{
    auto webMap = getObjectFromHandle<NGWWebMap>(object);
    if(nullptr == webMap) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWWebMap");
        return API_FALSE;
    }

    auto catalogObject = static_cast<Object*>(baseMap.baseMap);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    NGWWebMap::BaseMap in = {baseMap.opacity, baseMap.enabled == 1 ? true : false,
                             baseMap.displayName, catalogObject->pointer()};

    return webMap->addBaseMap(in) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsNGWWebMapInsertBaseMap Inset base map to web map
 * @param object Catalog object of type NGWWebMap.
 * @param baseMap Base map information as struct.
 * @param index Index to insert base map.
 * @return 1 on success, 0 if failed.
 */
char ngsNGWWebMapInsertBaseMap(CatalogObjectH object,
                               ngsNGWWebmapBasemapInfo baseMap, int index)
{
    auto webMap = getObjectFromHandle<NGWWebMap>(object);
    if(nullptr == webMap) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWWebMap");
        return API_FALSE;
    }

    auto catalogObject = static_cast<Object*>(baseMap.baseMap);
    if(!catalogObject) {
        errorMessage(_("The object handle is null"));
        return API_FALSE;
    }

    NGWWebMap::BaseMap in = {baseMap.opacity, baseMap.enabled == 1 ? true : false,
                             baseMap.displayName, catalogObject->pointer()};

    return webMap->insertBaseMap(index, in) ? API_TRUE : API_FALSE;
}

/**
 * @brief ngsNGWWebMapBaseMapList Base maps list of web map.
 * @param object Catalog object of type NGWWebMap.
 * @return List of ngsNGWWebmapBasemapInfo items or nullptr. The last element of list always nullptr.
 * The list must be freed using ngsFree function.
 */
ngsNGWWebmapBasemapInfo *ngsNGWWebMapBaseMapList(CatalogObjectH object)
{
    auto webMap = getObjectFromHandle<NGWWebMap>(object);
    if(nullptr == webMap) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWWebMap");
        return nullptr;
    }

    auto baseMaps = webMap->baseMaps();
    if(baseMaps.empty()) {
        return nullptr;
    }
    size_t outputSize = baseMaps.size();
    ngsNGWWebmapBasemapInfo *output =
            static_cast<ngsNGWWebmapBasemapInfo*>(
                CPLMalloc(sizeof(ngsNGWWebmapBasemapInfo) * (outputSize + 1)));
    clearCStrings();

    for(size_t i = 0; i < outputSize; ++i) {
        const auto &baseMap = baseMaps[i];
        output[i] = {baseMap.opacity, baseMap.enabled ? API_TRUE : API_FALSE,
                     storeCString(baseMap.displayName), baseMap.resource.get()};
    }

    output[outputSize] = {NOT_FOUND, false, nullptr, nullptr};
    return output;
}

/**
 * @brief ngsNGWWebMapDeleteItem Delete webmap item (group or layer)
 * @param object Catalog object of type NGWWebMap.
 * @param id Item identifier.
 * @return 1 on success, 0 if failed.
 */
char ngsNGWWebMapDeleteItem(CatalogObjectH object, long id)
{
    auto webMap = getObjectFromHandle<NGWWebMap>(object);
    if(nullptr == webMap) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWWebMap");
        return API_FALSE;
    }
    return webMap->deleteItem(id) ? API_TRUE : API_FALSE;
}

static ngsNGWWebmapLayerInfo *toWebMapLayerInfo(NGWWebMapLayer *layer)
{
    auto out = new ngsNGWWebmapLayerInfo;
    out->layer = layer->m_resource.get();
    out->adapter = storeCString(layer->m_adapter);
    out->enabled = layer->m_enabled ? 1 : 0;
    out->itemInfo.itemType = WMT_LAYER;
    out->itemInfo.displayName = storeCString(layer->m_displayName);
    out->transparency = layer->m_transparency;
    out->maxScaleDenom = storeCString(layer->m_maxScaleDenom);
    out->minScaleDenom = storeCString(layer->m_minScaleDenom);
    out->orderPosition = layer->m_orderPosition;
    return out;
}

static ngsNGWWebmapGroupInfo *toWebMapGroupInfo(NGWWebMapGroup *group)
{
    auto out = new ngsNGWWebmapGroupInfo;
    out->expanded = group->m_expanded ? 1 : 0;
    out->itemInfo.displayName = storeCString(group->m_displayName);
    out->itemInfo.itemType = group->m_type == NGWWebMapItem::ItemType::ROOT ? WMT_ROOT : WMT_GROUP;

    size_t outputSize = group->m_children.size();
    out->children = static_cast<ngsNGWWebmapItemInfo**>(
                CPLMalloc(sizeof(ngsNGWWebmapItemInfo*) * (outputSize + 1)));

    for(size_t i = 0; i < outputSize; ++i) {
        const auto &child = group->m_children[i];
        if(child->m_type == NGWWebMapItem::ItemType::LAYER) {
            auto childLayer = dynamic_cast<NGWWebMapLayer*>(child.get());
            out->children[i] = reinterpret_cast<ngsNGWWebmapItemInfo*>(toWebMapLayerInfo(childLayer));
        }
        else {
            auto childGroup = dynamic_cast<NGWWebMapGroup*>(child.get());
            out->children[i] = reinterpret_cast<ngsNGWWebmapItemInfo*>(toWebMapGroupInfo(childGroup));
        }
    }

    out->children[outputSize] = new ngsNGWWebmapItemInfo({WMT_UNKNOWN, nullptr});
    return out;
}

/**
 * @brief ngsNGWWebMapLayerTree
 * @param object Catalog object of type NGWWebMap.
 * @return Pointer to ngsNGWWebmapGroupInfo structure, Pointer must be freed using ngsNGWWebmapGroupInfoFree
 */
ngsNGWWebmapGroupInfo *ngsNGWWebMapLayerTree(CatalogObjectH object)
{
    auto webMap = getObjectFromHandle<NGWWebMap>(object);
    if(nullptr == webMap) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWWebMap");
        return nullptr;
    }
    clearCStrings();
    auto tree = webMap->layerTree();
    return toWebMapGroupInfo(tree.get());
}

static NGWWebMapLayer *fromWebMapLayerInfo(ngsNGWWebmapLayerInfo *layer,
                                           NGWWebMap *webMap)
{
    auto webMapLayer = new NGWWebMapLayer(webMap->connection());
    webMapLayer->m_adapter = fromCString(layer->adapter);
    webMapLayer->m_enabled = layer->enabled == 1;
    auto object = getObjectFromHandle<Object>(layer->layer);
    if(nullptr != object) {
        webMapLayer->m_resource = object->pointer();
    }
    webMapLayer->m_transparency = layer->transparency;
    webMapLayer->m_maxScaleDenom = fromCString(layer->maxScaleDenom);
    webMapLayer->m_minScaleDenom = fromCString(layer->minScaleDenom);
    webMapLayer->m_orderPosition = layer->orderPosition;
    webMapLayer->m_displayName = fromCString(layer->itemInfo.displayName);
    return webMapLayer;
}

static NGWWebMapGroup *fromWebMapGroupInfo(ngsNGWWebmapGroupInfo *group,
                                           NGWWebMap *webMap)
{
    if(nullptr == group) {
        return nullptr;
    }
    auto webMapGroup = new NGWWebMapGroup(webMap->connection());
    webMapGroup->m_expanded = group->expanded == 1;
    webMapGroup->m_displayName = fromCString(group->itemInfo.displayName);
    if(group->children) {
        int i = 0;
        ngsNGWWebmapItemInfo *child = nullptr;
        while((child = group->children[i++])->itemType != WMT_UNKNOWN) {
            if(child->itemType == WMT_LAYER) {
                webMapGroup->m_children.emplace_back(
                    fromWebMapLayerInfo(
                        reinterpret_cast<ngsNGWWebmapLayerInfo*>(child), webMap));
            }
            else if(child->itemType == WMT_GROUP) {
                webMapGroup->m_children.emplace_back(
                    fromWebMapGroupInfo(
                        reinterpret_cast<ngsNGWWebmapGroupInfo*>(child), webMap));
            }
            else {
                break;
            }
        }
    }
    return webMapGroup;
}

static NGWWebMapItemPtr fromWebMapItemInfo(ngsNGWWebmapItemInfo *item,
                                           NGWWebMap *webMap)
{
    if(nullptr == item || item->itemType == WMT_UNKNOWN) {
        return NGWWebMapItemPtr();
    }

    if(item->itemType == WMT_LAYER) {
        auto webMapItem = reinterpret_cast<ngsNGWWebmapLayerInfo*>(item);
        return NGWWebMapItemPtr(fromWebMapLayerInfo(webMapItem, webMap));
    }

    auto webMapItem = reinterpret_cast<ngsNGWWebmapGroupInfo*>(item);
    return NGWWebMapItemPtr(fromWebMapGroupInfo(webMapItem, webMap));
}
/**
 * @brief ngsNGWWebMapInsertItem Insert new item to web map
 * @param object Catalog object of type NGWWebMap.
 * @param pos Item identifier to insert new item. If identifier points to group, new item will add to the end of children list. If identifier points to layer, new item will add before this item. If identifier is equal -1, new item will add to the end of root children list.
 * @param item New item to insert.
 * @return New item identifier or -1 if failed.
 */
long ngsNGWWebMapInsertItem(CatalogObjectH object, long pos,
                            ngsNGWWebmapItemInfo *item)
{
    auto webMap = getObjectFromHandle<NGWWebMap>(object);
    if(nullptr == webMap) {
        errorMessage(_("Cannot cast to %s from input object"), "NGWWebMap");
        return NOT_FOUND;
    }
    return webMap->insetItem(pos, fromWebMapItemInfo(item, webMap).get());
}

/**
 * @brief ngsNGWWebmapItemInfoFree Free memory used by ngsNGWWebmapItemInfo structure
 * @param item ngsNGWWebmapItemInfo pointer
 */
void ngsNGWWebmapItemInfoFree(ngsNGWWebmapItemInfo *item)
{
    if(item->itemType == WMT_LAYER) {
        delete item;
    }
    auto group = reinterpret_cast<ngsNGWWebmapGroupInfo*>(item);
    ngsNGWWebmapGroupInfoFree(group);
}

/**
 * @brief ngsNGWWebmapGroupInfoFree Free memory used by ngsNGWWebmapGroupInfo structure
 * @param group ngsNGWWebmapGroupInfo pointer
 */
void ngsNGWWebmapGroupInfoFree(ngsNGWWebmapGroupInfo *group)
{
    if(group->children) {
        int i = 0;
        ngsNGWWebmapItemInfo *child = nullptr;
        while((child = group->children[i++])->itemType != WMT_UNKNOWN) {
            if(child->itemType == WMT_GROUP) {
                auto childGroup = reinterpret_cast<ngsNGWWebmapGroupInfo*>(child);
                ngsNGWWebmapGroupInfoFree(childGroup);
            }
        }
        ngsFree(group->children);
    }
}

