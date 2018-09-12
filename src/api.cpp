/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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
#include <cpl_http.h>

// gdal
#include "cpl_http.h"
#include "cpl_json.h"

#if __APPLE__
    #include "TargetConditionals.h"
#endif

#include "catalog/catalog.h"
#include "catalog/mapfile.h"
#include "ds/simpledataset.h"
#include "ds/storefeatureclass.h"
#include "map/mapstore.h"
#include "map/gl/layer.h"
#include "map/gl/overlay.h"
#include "ngstore/catalog/filter.h"
#include "ngstore/version.h"
#include "ngstore/util/constants.h"
#include "util/authstore.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/settings.h"
#include "util/stringutil.h"
#include "util/url.h"
#include "util/versionutil.h"

using namespace ngs;

constexpr const char *HTTP_TIMEOUT = "10";
constexpr const char *HTTP_USE_GZIP = "ON";
#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
constexpr const char *CACHEMAX = "4";
#elif __ANDROID__
constexpr const char *CACHEMAX = "4";
#else
constexpr const char *CACHEMAX = "64";
#endif

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

static void initGDAL(const char *dataPath, const char *cachePath)
{
    const Settings &settings = Settings::instance();
    // set config options
    if(dataPath) {
        CPLSetConfigOption("GDAL_DATA", dataPath);
        CPLDebug("ngstore", "GDAL_DATA path set to %s", dataPath);
    }

    CPLSetConfigOption("GDAL_CACHEMAX",
                       settings.getString("common/cachemax", CACHEMAX).c_str());
    CPLSetConfigOption("GDAL_HTTP_USERAGENT",
                       settings.getString("http/useragent", NGS_USERAGENT).c_str());
    CPLSetConfigOption("CPL_CURL_GZIP",
                       settings.getString("http/use_gzip", HTTP_USE_GZIP).c_str());
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT",
                       settings.getString("http/timeout", HTTP_TIMEOUT).c_str());
    CPLSetConfigOption("GDAL_DRIVER_PATH", "disabled");
#ifdef NGS_MOBILE // for mobile devices
    CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS",
                       settings.getString("gdal/CPL_VSIL_ZIP_ALLOWED_EXTENSIONS",
                                          ".apk, .ngmd").c_str());
#else
    CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS",
                       settings.getString("gdal/CPL_VSIL_ZIP_ALLOWED_EXTENSIONS",
                                          ".ngmd").c_str());
#endif
    if(cachePath) {
        CPLSetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", cachePath);
    }
    if(isDebugMode()) {
        CPLSetConfigOption("CPL_DEBUG", "ON");
        CPLSetConfigOption("CPL_CURL_VERBOSE", "ON");
    }

    CPLSetConfigOption("CPL_ZIP_ENCODING",
                       settings.getString("common/zip_encoding", "CP866").c_str());

    CPLDebug("ngstore", "HTTP user agent set to: %s", NGS_USERAGENT);

    CPLSetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "OFF");
    CPLSetConfigOption("GDAL_VALIDATE_OPEN_OPTIONS", "OFF");

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
#else
    GDALAllRegister();
#endif
    //SetCSVFilenameHook( CSVFileOverride );
}

static std::vector<std::vector<char>> cStrings;
static const char *storeCString(const std::string &str)
{
    std::vector<char> charName(str.begin(), str.end());
    charName.push_back('\0');
    cStrings.push_back(charName);
    return &cStrings.back()[0];
}

static void clearCStrings()
{
    if(cStrings.size() > 150) {
        cStrings.erase(cStrings.begin(), cStrings.begin() + 75);
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
 * - CACHE_DIR - path to cache directory (mainly for TMS/WMS cache)
 * - SETTINGS_DIR - path to settings directory
 * - GDAL_DATA - path to GDAL data directory (may be skipped on Linux)
 * - DEBUG_MODE ["ON", "OFF"] - May be ON or OFF strings to enable/disable debug mode
 * - LOCALE ["en_US.UTF-8", "de_DE", "ja_JP", ...] - Locale for error messages, etc.
 * - NUM_THREADS - Number threads in various functions (a positive number or "ALL_CPUS")
 * - GL_MULTISAMPLE - Enable sampling if applicable
 * - SSL_CERT_FILE - Path to ssl cert file (*.pem)
 * - HOME - Root directory for library
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsInit(char **options)
{
    gDebugMode = 0 != CSLFetchBoolean(options, "DEBUG_MODE", 0);
    CPLDebug("ngstore", "debug mode %s", gDebugMode ? "ON" : "OFF");
    const char *dataPath = CSLFetchNameValue(options, "GDAL_DATA");
    const char *cachePath = CSLFetchNameValue(options, "CACHE_DIR");
    const char *settingsPath = CSLFetchNameValue(options, "SETTINGS_DIR");
    if(settingsPath) {
        CPLSetConfigOption("NGS_SETTINGS_PATH", settingsPath);
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

#ifdef HAVE_LIBINTL_H
    const char* locale = CSLFetchNameValue(options, "LOCALE");
    //TODO: Do we need std::setlocale(LC_ALL, locale); execution here in library or it will call from programm?
#endif

#ifdef NGS_MOBILE
    if(nullptr == dataPath) {
        return outMessage(COD_NOT_SPECIFIED, _("GDAL_DATA option is required"));
    }
#endif

    initGDAL(dataPath, cachePath);

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

//------------------------------------------------------------------------------
// GDAL proxy functions
//------------------------------------------------------------------------------

/**
 * @brief ngsGetCurrentDirectory Returns curretnt working directory
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
 * @return List with added key=value string. List must bree freed using ngsDestroyList.
 */
char **ngsListAddNameValue(char **list, const char *name, const char *value)
{
    return CSLAddNameValue(list, name, value);
}

/**
 * @brief ngsDestroyList Destroy list created using ngsAddNameValue
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
 * @return The new path string
 */
const char *ngsFormFileName(const char *path, const char *name,
                            const char *extension)
{
    return CPLFormFilename(path, name, extension);
}

/**
 * @brief ngsFree Free pointer allocated using some function.
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
 * - POSTFIELDS=val, where val is a nul-terminated string to be passed to the server
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
 * @return structure of type ngsURLRequestResult
 */
ngsURLRequestResult *ngsURLRequest(enum ngsURLRequestType type, const char *url,
                                   char **options)
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

    ngsURLRequestResult *out = new ngsURLRequestResult;
    auto optionsPtr = requestOptions.asCharArray();
    CPLHTTPResult *result = CPLHTTPFetch(url, optionsPtr.get());

    if(result->nStatus != 0) {
        outMessage(COD_REQUEST_FAILED, result->pszErrBuf);
        out->status = result->nStatus;
        out->headers = nullptr;
        out->dataLen = 0;
        out->data = nullptr;

        CPLHTTPDestroyResult( result );

        return out;
    }
    else if(result->pszErrBuf) {
        outMessage(COD_WARNING, result->pszErrBuf);
    }

    unsigned char *buffer = new unsigned char[result->nDataLen];
    std::memcpy(buffer, result->pabyData, static_cast<size_t>(result->nDataLen));

    out->status = result->nStatus;
    out->headers = result->papszHeaders;
    out->dataLen = result->nDataLen;
    out->data = buffer;

    // Transfer own to out, don't delete with result
    result->papszHeaders = nullptr;

    CPLHTTPDestroyResult( result );

    return out;
}

ngsURLRequestResult *ngsURLUploadFile(const char *path, const char *url,
                                      char **options, ngsProgressFunc callback,
                                      void *callbackData)
{
    Options requestOptions(options);
    Progress progress(callback, callbackData);

    return uploadFile(fromCString(path), fromCString(url), progress, requestOptions);
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
    delete result->data;
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
 * HTTPAUTH_CLIENT_ID - Client identificator for bearer
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
    return AuthStore::addAuth(url, opt) ? COD_SUCCESS : COD_INSERT_FAILED;
}

/**
 * @brief ngsURLAuthGet If Authorization properties changed, this
 * function helps to get them back.
 * @param url The URL to search authorization options.
 * @return Key=value list (may be empty). User mast free returned
 * value via ngsDestroyList.
 */
char **ngsURLAuthGet(const char *url)
{
    Options option = AuthStore::description(fromCString(url));
    if(option.empty()) {
        return nullptr;
    }
    return option.asCharArray().release();
}

/**
 * @brief ngsURLAuthDelete Removes authorization from store
 * @param url The URL to search authorization options.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsURLAuthDelete(const char *url)
{
    AuthStore::deleteAuth(fromCString(url));
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
 * @param document JSON documetn hadler.
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
 * @param callbackData The data for callback function execution. May be NULL.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsJsonDocumentLoadUrl(JsonDocumentH document, const char *url, char **options,
                           ngsProgressFunc callback, void *callbackData)
{
    CPLJSONDocument *doc = static_cast<CPLJSONDocument*>(document);
    if(nullptr == doc) {
        return outMessage(COD_LOAD_FAILED, _("Layer pointer is null"));
    }

    Progress progress(callback, callbackData);
    return doc->LoadUrl(fromCString(url), options, ngsGDALProgress, &progress) ?
                COD_SUCCESS : COD_LOAD_FAILED;
}

/**
 * @brief ngsJsonDocumentRoot Gets JSON document root object.
 * @param document JSON document
 * @return JSON object handle or NULL. The handle must be deallocated using
 * ngsJsonObjectDestroy function.
 */
JsonObjectH ngsJsonDocumentRoot(JsonDocumentH document)
{
    CPLJSONDocument *doc = static_cast<CPLJSONDocument*>(document);
    if(nullptr == doc) {
        outMessage(COD_GET_FAILED, _("Layer pointer is null"));
        return nullptr;
    }
    return new CPLJSONObject(doc->GetRoot());
}


void ngsJsonObjectFree(JsonObjectH object)
{
    if(nullptr != object) {
        delete static_cast<CPLJSONObject*>(object);
    }
}

int ngsJsonObjectType(JsonObjectH object)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return CPLJSONObject::Type::Null;
    }
    return static_cast<CPLJSONObject*>(object)->GetType();
}

int ngsJsonObjectValid(JsonObjectH object)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return 0;
    }
    return static_cast<CPLJSONObject*>(object)->IsValid() ? 1 : 0;
}

const char *ngsJsonObjectName(JsonObjectH object)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return "";
    }
    return storeCString(static_cast<CPLJSONObject*>(object)->GetName());
}

JsonObjectH *ngsJsonObjectChildren(JsonObjectH object)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
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

void ngsJsonObjectChildrenListFree(JsonObjectH *list)
{
    if(nullptr == list) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
    }
    int counter = 0;
    CPLJSONObject *currentJsonObject;
    while((currentJsonObject = static_cast<CPLJSONObject*>(list[counter++])) !=
          nullptr) {
        delete currentJsonObject;
    }

    delete [] list;
}

const char *ngsJsonObjectGetString(JsonObjectH object, const char *defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }

    return storeCString(static_cast<CPLJSONObject*>(object)->ToString(
                            fromCString(defaultValue)));
}

double ngsJsonObjectGetDouble(JsonObjectH object, double defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->ToDouble(defaultValue);
}

int ngsJsonObjectGetInteger(JsonObjectH object, int defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->ToInteger(defaultValue);
}

long ngsJsonObjectGetLong(JsonObjectH object, long defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->ToLong(defaultValue);
}

int ngsJsonObjectGetBool(JsonObjectH object, int defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->ToBool(defaultValue) ? 1 : 0;
}


const char *ngsJsonObjectGetStringForKey(JsonObjectH object, const char *name,
                                         const char *defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return storeCString(static_cast<CPLJSONObject*>(object)->GetString(
                          fromCString(name), fromCString(defaultValue)));
}

double ngsJsonObjectGetDoubleForKey(JsonObjectH object, const char *name,
                                    double defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->GetDouble(fromCString(name),
                                                          defaultValue);
}

int ngsJsonObjectGetIntegerForKey(JsonObjectH object, const char *name,
                                  int defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->GetInteger(fromCString(name),
                                                           defaultValue);
}

long ngsJsonObjectGetLongForKey(JsonObjectH object, const char *name,
                                long defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->GetLong(fromCString(name),
                                                        defaultValue);
}

int ngsJsonObjectGetBoolForKey(JsonObjectH object, const char *name,
                               int defaultValue)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return defaultValue;
    }
    return static_cast<CPLJSONObject*>(object)->GetBool(fromCString(name),
                                                        defaultValue) ? 1 : 0;
}

int ngsJsonObjectSetStringForKey(JsonObjectH object, const char *name,
                                 const char* value)
{
    if(nullptr == object) {
        return outMessage(COD_GET_FAILED, _("The object handle is null"));
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), fromCString(value));
    return COD_SUCCESS;
}

int ngsJsonObjectSetDoubleForKey(JsonObjectH object, const char *name,
                                 double value)
{
    if(nullptr == object) {
        return outMessage(COD_GET_FAILED, _("The object handle is null"));
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), value);
    return COD_SUCCESS;
}

int ngsJsonObjectSetIntegerForKey(JsonObjectH object, const char* name,
                                  int value)
{
    if(nullptr == object) {
        return outMessage(COD_GET_FAILED, _("The object handle is null"));
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), value);
    return COD_SUCCESS;
}

int ngsJsonObjectSetLongForKey(JsonObjectH object, const char *name, long value)
{
    if(nullptr == object) {
        return outMessage(COD_GET_FAILED, _("The object handle is null"));
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), GInt64(value));
    return COD_SUCCESS;
}

int ngsJsonObjectSetBoolForKey(JsonObjectH object, const char *name, int value)
{
    if(nullptr == object) {
        return outMessage(COD_GET_FAILED, _("The object handle is null"));
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(object);
    gdalJsonObject->Set(fromCString(name), value);
    return COD_SUCCESS;
}

JsonObjectH ngsJsonObjectGetArray(JsonObjectH object, const char *name)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return nullptr;
    }
    return new CPLJSONArray(static_cast<CPLJSONObject*>(object)->GetArray(name));
}

JsonObjectH ngsJsonObjectGetObject(JsonObjectH object, const char *name)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return nullptr;
    }
    return new CPLJSONObject(
                static_cast<CPLJSONObject*>(object)->GetObj(fromCString(name)));
}

int ngsJsonArraySize(JsonObjectH object)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return 0;
    }
    return static_cast<CPLJSONArray*>(object)->Size();
}

JsonObjectH ngsJsonArrayItem(JsonObjectH object, int index)
{
    if(nullptr == object) {
        outMessage(COD_GET_FAILED, _("The object handle is null"));
        return nullptr;
    }
    return new CPLJSONObject(static_cast<CPLJSONArray*>(object)->operator[](index));
}


//------------------------------------------------------------------------------
// Catalog
//------------------------------------------------------------------------------

/**
 * @brief catalogObjectQuery Request the contents of some catalog conatiner object.
 * @param object Catalog object. Must be container (containes some catalog objects).
 * @param objectFilter Filter output results
 * @return List of ngsCatalogObjectInfo items or NULL. The last element of list always NULL.
 * The list must be deallocated using ngsFree function.
 */
static ngsCatalogObjectInfo *catalogObjectQuery(CatalogObjectH object,
                                                const Filter &objectFilter)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }
    ObjectPtr catalogObjectPointer = catalogObject->pointer();

    ngsCatalogObjectInfo *output = nullptr;
    size_t outputSize = 0;
    ObjectContainer * const container = dynamic_cast<ObjectContainer*>(catalogObject);
    if(!container) {
//        if(!objectFilter.canDisplay(catalogObjectPointer)) {
            return nullptr;
//        }

//        output = static_cast<ngsCatalogObjectInfo*>(
//                    CPLMalloc(sizeof(ngsCatalogObjectInfo) * 2));
//        output[0] = {catalogObject->name(), catalogObject->type(), catalogObject};
//        output[1] = {nullptr, -1, nullptr};
//        return output;
    }

    clearCStrings();

    container->loadChildren();
    if(!container->hasChildren()) {
        if(container->type() == CAT_CONTAINER_SIMPLE) {
            SimpleDataset * const simpleDS = dynamic_cast<SimpleDataset*>(container);
            output = static_cast<ngsCatalogObjectInfo*>(
                                    CPLMalloc(sizeof(ngsCatalogObjectInfo) * 2));
            output[0] = {storeCString(catalogObject->name()),
                         simpleDS->subType(), catalogObject};
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
            SimpleDataset *simpleDS = nullptr;
            if(child->type() == CAT_CONTAINER_SIMPLE) {
                simpleDS = ngsDynamicCast(SimpleDataset, child);
                if(simpleDS && !simpleDS->loadChildren()) {
                    continue;
                }
            }

            outputSize++;
            output = static_cast<ngsCatalogObjectInfo*>(CPLRealloc(output,
                                sizeof(ngsCatalogObjectInfo) * (outputSize + 1)));

            const char *name = storeCString(child->name());
            if(simpleDS) {
                output[outputSize - 1] = {name, simpleDS->subType(),
                                          simpleDS->internalObject().get()};
            }
            else {
                output[outputSize - 1] = {name, child->type(), child.get()};
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
 * @return Array of ngsCatlogObjectInfo structures. Caller must free this array
 * after using with ngsFree method
 */
ngsCatalogObjectInfo *ngsCatalogObjectQuery(CatalogObjectH object, int filter)
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
 * @brief ngsCatalogObjectDelete Deletes catalog object on specified path
 * @param object The handle of catalog object
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsCatalogObjectDelete(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
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
 * @brief ngsCatalogObjectCreate Creates new catalog object
 * @param object The handle of catalog object
 * @param name The new object name
 * @param options The array of create object options. Caller must free this
 * array after function finishes. The common values are:
 * TYPE (required) - The new object type from enum ngsCatalogObjectType
 * CREATE_UNIQUE [ON, OFF] - If name already exists in container, make it unique
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsCatalogObjectCreate(CatalogObjectH object, const char *name, char **options)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }

    Options createOptions(options);
    enum ngsCatalogObjectType type = static_cast<enum ngsCatalogObjectType>(
                createOptions.asInt("TYPE", CAT_UNKNOWN));
    createOptions.remove("TYPE");
    ObjectContainer * const container = dynamic_cast<ObjectContainer*>(catalogObject);
    if(nullptr == container) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }

    // If dataset - open it.
    DatasetBase *datasetBase = dynamic_cast<DatasetBase*>(container);
    if(datasetBase && !datasetBase->isOpened()) {
        datasetBase->open();
    }

    // Check can create
    if(container->canCreate(type)) {
        return container->create(type, fromCString(name), createOptions) ?
                    COD_SUCCESS : COD_CREATE_FAILED;
    }

    return outMessage(COD_UNSUPPORTED,
                      _("Cannot create such object type (%d) in path: %s"),
                      type, catalogObject->fullName().c_str());
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
 * @brief ngsCatalogObjectCopy Copies or moves catalog object to another location
 * @param srcObject Source catalog object
 * @param dstObjectContainer Destination catalog container
 * @param options Copy options. This is a list of key=value items.
 * The common value is MOVE=ON indicating to move object. The other values depends on
 * destination container.
 * @param callback Progress function (template is ngsProgressFunc) executed
 * periodically to report progress and cancel. If returns 1 the execution will
 * continue, 0 - cancelled.
 * @param callbackData Progress fuction parameter.
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

    // If dataset - open it.
    DatasetBase *datasetBase = dynamic_cast<DatasetBase*>(dstCatalogObjectContainer);
    if(datasetBase && !datasetBase->isOpened()) {
        datasetBase->open();
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
 * @param object The handle of catalog object
 * @param optionType The one of ngsOptionTypes enum values:
 * OT_CREATE_DATASOURCE, OT_CREATE_RASTER, OT_CREATE_LAYER,
 * OT_CREATE_LAYER_FIELD, OT_OPEN, OT_LOAD
 * @return Options description in xml form.
 */
const char *ngsCatalogObjectOptions(CatalogObjectH object, int optionType)
{
    if(!object) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return "";
    }
    Object *catalogObject = static_cast<Object*>(object);
    if(!Filter::isDatabase(catalogObject->type())) {
        outMessage(COD_INVALID,
                    _("The input object not a dataset. The type is %d. Options query not supported"),
                    catalogObject->type());
        return "";
    }

    Dataset * const dataset = dynamic_cast<Dataset*>(catalogObject);
    if(nullptr == dataset) {
        outMessage(COD_INVALID,
                    _("The input object not a dataset. Options query not supported"));
        return "";
    }
    enum ngsOptionType enumOptionType = static_cast<enum ngsOptionType>(optionType);

    return storeCString(dataset->options(enumOptionType));
}

/**
 * @brief ngsCatalogObjectGet Gets catalog object handle by path
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
 * @brief ngsCatalogObjectMetadata Returns catalog object metadata
 * @param object Catalog object the metadata requested
 * @param domain The metadata specific domain or NULL
 * @return The list of key=value items or NULL. The last item of list always NULL.
 */
char **ngsCatalogObjectProperties(CatalogObjectH object, const char *domain)
{
    if(nullptr == object) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }

    Properties propeties = static_cast<Object*>(object)->properties(
                fromCString(domain));

    return propeties.asCharArray().release();
}

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
        outMessage(COD_INVALID, _("The object handle is null"));
        return;
    }

    ObjectContainer *container = dynamic_cast<ObjectContainer*>(catalogObject);
    if(!container) {
        outMessage(COD_INVALID, _("The object is not container"));
        return;
    }
    container->refresh();
}

//------------------------------------------------------------------------------
// Feature class
//------------------------------------------------------------------------------

static FeatureClass *getFeatureClassFromHandle(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }

    ObjectPtr catalogObjectPointer = catalogObject->pointer();
    if(!catalogObjectPointer) {
        outMessage(COD_INVALID, _("Source dataset type is incompatible"));
        return nullptr;
    }

    if(catalogObjectPointer->type() == CAT_CONTAINER_SIMPLE) {
        SimpleDataset * const dataset = dynamic_cast<SimpleDataset*>(catalogObject);
        if(dataset->loadChildren()) {
            catalogObjectPointer = dataset->internalObject();
        }
    }


    if(!Filter::isFeatureClass(catalogObjectPointer->type())) {
        outMessage(COD_INVALID, _("Source dataset type is incompatible"));
        return nullptr;
    }

    return ngsDynamicCast(FeatureClass, catalogObjectPointer);
}

static Table *getTableFromHandle(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }

    ObjectPtr catalogObjectPointer = catalogObject->pointer();
    if(!catalogObjectPointer) {
        outMessage(COD_INVALID, _("Source dataset type is incompatible"));
        return nullptr;
    }

    if(catalogObjectPointer->type() == CAT_CONTAINER_SIMPLE) {
        SimpleDataset * const dataset = dynamic_cast<SimpleDataset*>(catalogObject);
        if(dataset->loadChildren()) {
            catalogObjectPointer = dataset->internalObject();
        }
    }

    if(!(Filter::isTable(catalogObjectPointer->type()) ||
         Filter::isFeatureClass(catalogObjectPointer->type()))) {
        outMessage(COD_INVALID, _("Source dataset type is incompatible"));
        return nullptr;
    }

    return ngsDynamicCast(Table, catalogObjectPointer);
}

static Raster *getRasterFromHandle(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }

    ObjectPtr catalogObjectPointer = catalogObject->pointer();
    if(!catalogObjectPointer) {
        outMessage(COD_INVALID, _("Source dataset type is incompatible"));
        return nullptr;
    }

    if(!(Filter::isRaster(catalogObjectPointer->type()))) {
        outMessage(COD_INVALID, _("Source dataset type is incompatible"));
        return nullptr;
    }

    return ngsDynamicCast(Raster, catalogObjectPointer);
}

static DatasetBase *getDataseFromHandle(CatalogObjectH object)
{
    Object *catalogObject = static_cast<Object*>(object);
    if(!catalogObject) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }

    ObjectPtr catalogObjectPointer = catalogObject->pointer();
    if(!catalogObjectPointer) {
        outMessage(COD_INVALID, _("Source dataset type is incompatible"));
        return nullptr;
    }

    return ngsDynamicCast(DatasetBase, catalogObjectPointer);
}

int ngsDatasetOpen(CatalogObjectH object, unsigned int openFlags, char **openOptions)
{
    DatasetBase *dataset = getDataseFromHandle(object);
    if(!dataset) {
        return COD_OPEN_FAILED;
    }

    return dataset->open(openFlags, Options(openOptions)) ? COD_SUCCESS :
                                                            COD_OPEN_FAILED;
}

char ngsDatasetIsOpened(CatalogObjectH object)
{
    DatasetBase *dataset = getDataseFromHandle(object);
    if(!dataset) {
        return 0;
    }

    return dataset->isOpened() ? 1 : 0;
}

int ngsDatasetClose(CatalogObjectH object)
{
    DatasetBase *dataset = getDataseFromHandle(object);
    if(!dataset) {
        return COD_CLOSE_FAILED;
    }
    dataset->close();
    return COD_SUCCESS;
}

/**
 * @brief ngsFeatureClassFields Feature class fields
 * @param object Feature class handle
 * @return NULL or list of ngsField structure pointers. The list last element
 * always has nullptr name and alias, The list must be deallocated using ngsFree
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
                CPLMalloc(size_t(fields.size() + 1) * sizeof(ngsField)));

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
        return 0;
    }
    return featureClass->geometryType();
}

/**
 * @brief ngsFeatureClassCreateOverviews Creates Gl optimized vector tiles
 * @param object Catalog object handle. Must be feature class or simple datasource.
 * @param options The options key-value array specific to operation.
 * @param callback The callback function to report or cancel process.
 * @param callbackData The callback function data.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsFeatureClassCreateOverviews(CatalogObjectH object, char **options,
                                   ngsProgressFunc callback, void *callbackData)
{
    FeatureClass *featureClass = getFeatureClassFromHandle(object);
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
        outMessage(COD_INVALID, _("The object handle is null"));
        return;
    }

    Dataset *dataset = dynamic_cast<Dataset*>(catalogObject);
    if(!dataset) {
        Table *table = getTableFromHandle(object);
        if(!table) {
            outMessage(COD_INVALID, _("Source dataset type is incompatible"));
            return;
        }

        dataset = dynamic_cast<Dataset*>(table->parent());
        if(!dataset) {
            outMessage(COD_INVALID, _("Source dataset type is incompatible"));
            return;
        }
    }

    if(!dataset->isOpened()) {
        dataset->open();
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
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
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
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
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
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
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
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
 * @param id Feature identificator to delete
 * @param logEdits If log edit operation enabled this key can manage to log or
 * not this edit operation
 * @return COD_SUCCESS if everything is OK
 */
int ngsFeatureClassDeleteFeature(CatalogObjectH object, long long id,
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
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
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
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
 * @return Feature/Row count
 */
long long ngsFeatureClassCount(CatalogObjectH object)
{
    Table *table = getTableFromHandle(object);
    if(nullptr == table) {
        return 0;
    }
    return table->featureCount();
}

/**
 * @brief ngsFeatureClassResetReading Move read cursor to the begin
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
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
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
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
 * @brief ngsFeatureClassGetFeature Returns feature by identificator
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
 * @param id Feature identificator
 * @return Feature handle
 */
FeatureH ngsFeatureClassGetFeature(CatalogObjectH object, long long id)
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
 * @param object Handle to Table, FeatureClass or SimpleDataset catalog object
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
                GeometryPtr(static_cast<OGRGeometry*>(geometryFilter)->clone()));
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
    Table *table = getTableFromHandle(object);
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
        outMessage(COD_INVALID, _("The object handle is null"));
        return 0;
    }
    return (*featurePtrPointer)->GetFieldCount();
}

char ngsFeatureIsFieldSet(FeatureH feature, int fieldIndex)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return 0;
    }
    return (*featurePtrPointer)->IsFieldSetAndNotNull(fieldIndex) ? 1 : 0;

}

long long ngsFeatureGetId(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return NOT_FOUND;
    }
    return (*featurePtrPointer)->GetFID();
}

GeometryH ngsFeatureGetGeometry(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }
    return (*featurePtrPointer)->GetGeometryRef()->clone();
}

int ngsFeatureGetFieldAsInteger(FeatureH feature, int field)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return 0;
    }
    return (*featurePtrPointer)->GetFieldAsInteger(field);

}

double ngsFeatureGetFieldAsDouble(FeatureH feature, int field)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return 0.0;
    }
    return (*featurePtrPointer)->GetFieldAsDouble(field);
}

const char *ngsFeatureGetFieldAsString(FeatureH feature, int field)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return "";
    }
    return (*featurePtrPointer)->GetFieldAsString(field);
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
        outMessage(COD_INVALID, _("The object handle is null"));
        return;
    }

    // TODO: Reproject if differs srs

    (*featurePtrPointer)->SetGeometry(static_cast<OGRGeometry*>(geometry));
}

void ngsFeatureSetFieldInteger(FeatureH feature, int field, int value)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return;
    }
    (*featurePtrPointer)->SetField(field, value);
}

void ngsFeatureSetFieldDouble(FeatureH feature, int field, double value)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return;
    }
    (*featurePtrPointer)->SetField(field, value);
}

void ngsFeatureSetFieldString(FeatureH feature, int field, const char* value)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
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
        outMessage(COD_INVALID, _("The object handle is null"));
        return;
    }
    (*featurePtrPointer)->SetField(field, year, month, day, hour, minute, second,
                                   TZFlag);
}

//------------------------------------------------------------------------------
// StoreFeature
//------------------------------------------------------------------------------

long long ngsStoreFeatureGetRemoteId(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return NOT_FOUND;
    }
    return StoreTable::getRemoteId(*featurePtrPointer);
}


void ngsStoreFeatureSetRemoteId(FeatureH feature, long long rid)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return;
    }
    StoreTable::setRemoteId(*featurePtrPointer, rid);
}

FeatureH ngsStoreFeatureClassGetFeatureByRemoteId(CatalogObjectH object,
                                                  long long rid)
{
    StoreFeatureClass *featureClass = dynamic_cast<StoreFeatureClass*>(
                getFeatureClassFromHandle(object));
    if(!featureClass) {
        outMessage(COD_INVALID, _("Source dataset type is incompatible"));
        return nullptr;
    }
    FeaturePtr remoteIdFeature = featureClass->getFeatureByRemoteId(rid);
    if(remoteIdFeature) {
        return new FeaturePtr(remoteIdFeature);
    }
    return nullptr;
}

GeometryH ngsFeatureCreateGeometry(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }
    OGRGeomFieldDefn *defn = (*featurePtrPointer)->GetGeomFieldDefnRef(0);
    return OGRGeometryFactory::createGeometry(defn->GetType());
}

GeometryH ngsFeatureCreateGeometryFromJson(JsonObjectH geometry)
{
    CPLJSONObject *jsonGeometry = static_cast<CPLJSONObject*>(geometry);
    if(!jsonGeometry) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }

    return ngsCreateGeometryFromGeoJson(*jsonGeometry);
}

/**
 * @brief ngsGeometryFree Free geometry handle. Only usefull if geometry created,
 * but not addet to feature
 * @param geometry Geometry handle
 */
void ngsGeometryFree(GeometryH geometry)
{
    OGRGeometry *geom = static_cast<OGRGeometry*>(geometry);
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
    OGRSpatialReference to;
    if(to.importFromEPSG(EPSG) != OGRERR_NONE) {
        return outMessage(COD_UNSUPPORTED, _("Unsupported from EPSG with code %d"),
                          EPSG);
    }

    return static_cast<OGRGeometry*>(geometry)->transformTo(&to) == OGRERR_NONE ?
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
    return static_cast<OGRGeometry*>(geometry)->IsEmpty() ? 1 : 0;
}

ngsGeometryType ngsGeometryGetType(GeometryH geometry)
{
    return static_cast<OGRGeometry*>(geometry)->getGeometryType();
}

const char *ngsGeometryToJson(GeometryH geometry)
{
    return static_cast<OGRGeometry*>(geometry)->exportToJson();
}

CoordinateTransformationH ngsCoordinateTransformationCreate(int fromEPSG,
                                                            int toEPSG)
{
    if(fromEPSG == toEPSG) {
        outMessage(COD_INVALID, _("From/To EPSG codes are equal"));
        return nullptr;
    }

    OGRSpatialReference from;
    if(from.importFromEPSG(fromEPSG) != OGRERR_NONE) {
        outMessage(COD_UNSUPPORTED, _("Unsupported from EPSG with code %d"),
                     fromEPSG);
        return nullptr;
    }

    OGRSpatialReference to;
    if(to.importFromEPSG(toEPSG) != OGRERR_NONE) {
        outMessage(COD_UNSUPPORTED, _("Unsupported from EPSG with code %d"),
                     toEPSG);
        return nullptr;
    }

    return OGRCreateCoordinateTransformation(&from, &to);
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
        outMessage(COD_INVALID, _("The object handle is null"));
        return {0.0, 0.0, 0.0};
    }

    pct->Transform(1, &coordinates.X, &coordinates.Y, &coordinates.Z);
    return coordinates;
}

long long ngsFeatureAttachmentAdd(FeatureH feature, const char *name,
                                  const char *description, const char *path,
                                  char **options, char logEdits)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return NOT_FOUND;
    }
    Table *table = featurePtrPointer->table();
    if(!table) {
        outMessage(COD_INVALID, _("The feature detached from table"));
        return NOT_FOUND;
    }

    if(table->type() == CAT_QUERY_RESULT || table->type() == CAT_QUERY_RESULT_FC) {
        outMessage(COD_INVALID, _("The feature from table that is result of query"));
        return NOT_FOUND;
    }

    GIntBig fid = (*featurePtrPointer)->GetFID();
    return table->addAttachment(fid, fromCString(name), fromCString(description),
                                fromCString(path), Options(options),
                                logEdits == 1);
}

int ngsFeatureAttachmentDeleteAll(FeatureH feature, char logEdits)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }

    Table *table = featurePtrPointer->table();
    if(!table) {
        return outMessage(COD_INVALID, _("The feature detached from table"));
    }

    if(table->type() == CAT_QUERY_RESULT || table->type() == CAT_QUERY_RESULT_FC) {
        return outMessage(COD_INVALID, _("The feature from table that is result of query"));
    }

    return table->deleteAttachments(logEdits == 1);
}

int ngsFeatureAttachmentDelete(FeatureH feature, long long aid, char logEdits)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }

    Table *table = featurePtrPointer->table();
    if(!table) {
        return outMessage(COD_INVALID, _("The feature detached from table"));
    }

    if(table->type() == CAT_QUERY_RESULT || table->type() == CAT_QUERY_RESULT_FC) {
        return outMessage(COD_INVALID, _("The feature from table that is result of query"));
    }

    return table->deleteAttachment(aid, logEdits == 1);
}

static std::vector<Table::AttachmentInfo> info;
ngsFeatureAttachmentInfo *ngsFeatureAttachmentsGet(FeatureH feature)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return nullptr;
    }

    Table *table = featurePtrPointer->table();
    if(!table) {
        outMessage(COD_INVALID, _("The feature detached from table"));
        return nullptr;
    }

    if(table->type() == CAT_QUERY_RESULT || table->type() == CAT_QUERY_RESULT_FC) {
        outMessage(COD_INVALID, _("The feature from table that is result of query"));
        return nullptr;
    }

    clearCStrings();

    GIntBig fid = (*featurePtrPointer)->GetFID();
    info = table->attachments(fid);
    ngsFeatureAttachmentInfo *out = static_cast<ngsFeatureAttachmentInfo*>(
                CPLMalloc((info.size() + 1) * sizeof(ngsFeatureAttachmentInfo)));
    int counter = 0;
    for(const Table::AttachmentInfo &infoItem : info) {
        ngsFeatureAttachmentInfo outInfo = {infoItem.id,
                                            storeCString(infoItem.name),
                                            storeCString(infoItem.description),
                                            storeCString(infoItem.path),
                                            infoItem.size, infoItem.rid};

        out[counter++] = outInfo;
    }

    out[info.size()] = {-1, nullptr, nullptr, nullptr, 0, -1};
    return out;
}

int ngsFeatureAttachmentUpdate(FeatureH feature, long long aid,
                               const char *name, const char *description,
                               char logEdits)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        return outMessage(COD_INVALID, _("The object handle is null"));
    }

    Table *table = featurePtrPointer->table();
    if(!table) {
        return outMessage(COD_INVALID, _("The feature detached from table"));
    }

    if(table->type() == CAT_QUERY_RESULT || table->type() == CAT_QUERY_RESULT_FC) {
        return outMessage(COD_INVALID, _("The feature from table that is result of query"));
    }

    return table->updateAttachment(aid, fromCString(name), fromCString(description),
                                   logEdits == 1) ? COD_SUCCESS : COD_UPDATE_FAILED;
}

void ngsStoreFeatureSetAttachmentRemoteId(FeatureH feature, long long aid,
                                          long long rid)
{
    FeaturePtr *featurePtrPointer = static_cast<FeaturePtr*>(feature);
    if(!featurePtrPointer) {
        outMessage(COD_INVALID, _("The object handle is null"));
        return;
    }

    StoreFeatureClass *table = dynamic_cast<StoreFeatureClass*>(
                featurePtrPointer->table());
    if(!table) {
        outMessage(COD_INVALID, _("The feature detached from table"));
        return;
    }

    table->setFeatureAttachmentRemoteId(aid, rid);
}

//------------------------------------------------------------------------------
// Raster
//------------------------------------------------------------------------------

/**
 * @brief ngsRasterCacheArea Download tiles for zoom levels and area
 * @param object Raster to download tiles
 * @param options Key=value list of options.
 * - MINX - minimum X coordinate of bounting box
 * - MINY - minimum Y coordinate of bounting box
 * - MAXX - maximum X coordinate of bounting box
 * - MAXY - maximum Y coordinate of bounting box
 * - ZOOM_LEVELS - comma separated values of zoom levels
 * @param callback Progress function
 * @param callbackData Progress function arguments
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

    return raster->cacheArea(createProgress, createOptions) ?
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
 * @return 0 if create failed or map identificator.
 */
unsigned char ngsMapCreate(const char *name, const char *description,
                 unsigned short epsg, double minX, double minY,
                 double maxX, double maxY)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return MapStore::invalidMapId();
    }
    Envelope bound(minX, minY, maxX, maxY);
    return mapStore->createMap(fromCString(name), fromCString(description), epsg,
                               bound);
}

/**
 * @brief ngsMapOpen Opens existing map from file
 * @param path Path to map file inside catalog in form ngc://some path/
 * @return 0 if open failed or map id.
 */
unsigned char ngsMapOpen(const char *path)
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
 * @param mapId Map identificator to save
 * @param path Path to store map data
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSave(unsigned char mapId, const char *path)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return MapStore::invalidMapId();
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

    if(!mapFile || !mapStore->saveMap(mapId, mapFile)) {
        return COD_SAVE_FAILED;
    }

    return COD_SUCCESS;
}

/**
 * @brief ngsMapClose Closes map and free resources
 * @param mapId Map identificator to close
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapClose(unsigned char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_CLOSE_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->closeMap(mapId) ? COD_SUCCESS : COD_CLOSE_FAILED;
}


/**
 * @brief ngsMapSetSize Sets map size in pixels
 * @param mapId Map identificator received from create or open map functions
 * @param width Output image width
 * @param height Output image height
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetSize(unsigned char mapId, int width, int height, int isYAxisInverted)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_CLOSE_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->setMapSize(mapId, width, height, isYAxisInverted == 1 ?
                                    true : false) ? COD_SUCCESS : COD_SET_FAILED;
}

/**
 * @brief ngsDrawMap Starts drawing map in specified (in ngsInitMap) extent
 * @param mapId Map identificator received from create or open map functions
 * @param state Draw state (NORMAL, PRESERVED, REDRAW)
 * @param callback Progress function
 * @param callbackData Progress function arguments
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapDraw(unsigned char mapId, enum ngsDrawState state,
               ngsProgressFunc callback, void *callbackData)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DRAW_FAILED, _("MapStore is not initialized"));
    }
    Progress progress(callback, callbackData);
    return mapStore->drawMap(mapId, state, progress) ? COD_SUCCESS : COD_DRAW_FAILED;
}

int ngsMapInvalidate(unsigned char mapId, ngsExtent bounds)
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
 * @param mapId Map identificator received from create or open map functions
 * @return map background color struct
 */
ngsRGBA ngsMapGetBackgroundColor(unsigned char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return {0,0,0,0};
    }
    return mapStore->getMapBackgroundColor(mapId);
}

/**
 * @brief ngsSetMapBackgroundColor Sets map background color
 * @param mapId Map identificator received from create or open map functions
 * @param color Background color
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetBackgroundColor(unsigned char mapId, ngsRGBA color)
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
 * @param mapId Map identificator
 * @param x X coordinate
 * @param y Y coordinate
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetCenter(unsigned char mapId, double x, double y)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_SET_FAILED,  _("MapStore is not initialized"));
    }
    return mapStore->setMapCenter(mapId, x, y) ? COD_SUCCESS : COD_SET_FAILED;
}

/**
 * @brief ngsMapGetCenter Gets map center for current view (extent)
 * @param mapId Map identificator
 * @return Coordinate structure. If error occurred all coordinates set to 0.0
 */
ngsCoordinate ngsMapGetCenter(unsigned char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return {0, 0, 0};
    }
    return mapStore->getMapCenter(mapId);
}

/**
 * @brief ngsMapGetCoordinate Geographic coordinates for display position
 * @param mapId Map identificator
 * @param x X position
 * @param y Y position
 * @return Geographic coordinates
 */
ngsCoordinate ngsMapGetCoordinate(unsigned char mapId, double x, double y)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return {0, 0, 0};
    }
    return mapStore->getMapCoordinate(mapId, x, y);
}

/**
 * @brief ngsMapSetScale Sets current map scale
 * @param mapId Map identificator
 * @param scale value to set
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetScale(unsigned char mapId, double scale)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_SET_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->setMapScale(mapId, scale);
}

/**
 * @brief ngsMapGetScale Returns current map scale
 * @param mapId Map identificator
 * @return Current map scale or 1
 */
double ngsMapGetScale(unsigned char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return 1.0;
    }
    return mapStore->getMapScale(mapId);
}

/**
 * @brief ngsMapCreateLayer Creates new layer in map
 * @param mapId Map identificator
 * @param name Layer name
 * @param path Path to map file inside catalog in form ngc://some path/
 * @return Layer Id or -1
 */
int ngsMapCreateLayer(unsigned char mapId, const char *name, const char *path)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_CREATE_FAILED, _("MapStore is not initialized"));
        return NOT_FOUND;
    }

    CatalogPtr catalog = Catalog::instance();
    ObjectPtr object = catalog->getObject(path);
    if(!object) {
        outMessage(COD_INVALID, _("Source dataset '%s' not found"), path);
        return NOT_FOUND;
    }

    return mapStore->createLayer(mapId, fromCString(name), object);
}

/**
 * @brief ngsMapLayerReorder Reorders layers in map
 * @param mapId Map identificator
 * @param beforeLayer Before this layer insert movedLayer. May be null. In that
 * case layer will be moved to the end of map
 * @param movedLayer Layer to move
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapLayerReorder(unsigned char mapId, LayerH beforeLayer, LayerH movedLayer)
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
 * @param mapId Map identificator
 * @param dir Rotate direction. May be X, Y or Z
 * @param rotate value to set
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetRotate(unsigned char mapId, ngsDirection dir, double rotate)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_INVALID, _("MapStore is not initialized"));
    }
    return mapStore->setMapRotate(mapId, dir, rotate);
}

/**
 * @brief ngsMapGetRotate Returns map rotate value
 * @param mapId Map identificator
 * @param dir Rotate direction. May be X, Y or Z
 * @return rotate value or 0 if error occured
 */
double ngsMapGetRotate(unsigned char mapId, ngsDirection dir)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return 0.0;
    }
    return mapStore->getMapRotate(mapId, dir);
}

/**
 * @brief ngsMapGetDistance Map distance from display length
 * @param mapId Map identificator
 * @param w Width
 * @param h Height
 * @return ngsCoordinate where X distance along x axis and Y along y axis
 */
ngsCoordinate ngsMapGetDistance(unsigned char mapId, double w, double h)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return {0.0, 0.0, 0.0};
    }
    return mapStore->getMapDistance(mapId, w, h);
}

/**
 * @brief ngsMapLayerCount Returns layer count in map
 * @param mapId Map identificator
 * @return Layer count in map
 */
int ngsMapLayerCount(unsigned char mapId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return 0;
    }
    return static_cast<int>(mapStore->getLayerCount(mapId));
}

/**
 * @brief ngsMapLayerGet Returns map layer handle
 * @param mapId Map identificator
 * @param layerId Layer index
 * @return Layer handle. The caller should not delete it.
 */
LayerH ngsMapLayerGet(unsigned char mapId, int layerId)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return nullptr;
    }
    return mapStore->getLayer(mapId, layerId).get();
}

/**
 * @brief ngsMapLayerDelete Deletes layer from map
 * @param mapId Map identificator
 * @param layer Layer handle get from ngsMapLayerGet() function.
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapLayerDelete(unsigned char mapId, LayerH layer)
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
 * @param mapId Map identificator
 * @param options Key=Value list of options. Available options are:
 *   ZOOM_INCREMENT - Add integer value to zomm level correspondent to scale. May be negative
 *   VIEWPORT_REDUCE_FACTOR - Reduce view size on provided value. Make sense to
 *     reduce number of tiles in map extent. The tiles will be more pixelate
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetOptions(unsigned char mapId, char **options)
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
 * @param mapId Map identificator
 * @param minX Minimum X coordinate
 * @param minY Minimum Y coordinate
 * @param maxX Maximum X coordinate
 * @param maxY Maximum Y coordinate
 * @return ngsCode value - COD_SUCCESS if everything is OK
 */
int ngsMapSetExtentLimits(unsigned char mapId, double minX, double minY,
                                      double maxX, double maxY)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DELETE_FAILED, _("MapStore is not initialized"));
    }

    Envelope extentLimits(minX, minY, maxX, maxY);
    return mapStore->setExtentLimits(mapId, extentLimits) ?
                COD_SUCCESS : COD_SET_FAILED;
}

ngsExtent ngsMapGetExtent(unsigned char mapId, int epsg)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_DELETE_FAILED, _("MapStore is not initialized"));
        return {0.0, 0.0, 0.0, 0.0};
    }

    auto map = mapStore->getMap(mapId);
    if(map) {
        unsigned short fromEPSG = map->epsg();
        Envelope env = map->getExtent();

        if(fromEPSG == epsg) {
            return {env.minX(), env.minY(), env.maxX(), env.maxY()};
        }

        OGRSpatialReference from;
        if(from.importFromEPSG(fromEPSG) != OGRERR_NONE) {
            outMessage(COD_UNSUPPORTED, _("Unsupported from EPSG with code %d"),
                     fromEPSG);
            return {0.0, 0.0, 0.0, 0.0};
        }

        OGRSpatialReference to;
        if(to.importFromEPSG(epsg) != OGRERR_NONE) {
            outMessage(COD_UNSUPPORTED, _("Unsupported from EPSG with code %d"),
                         epsg);
            return {0.0, 0.0, 0.0, 0.0};
        }

        OGRCoordinateTransformation* ct =
                OGRCreateCoordinateTransformation(&from, &to);
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

int ngsMapSetExtent(unsigned char mapId, ngsExtent extent)
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
 * @param mapId Map identificator
 * @param styleType Style type (Point, Line or fill)
 * @return NULL or JSON style handle. The handle must be freed by ngsJsonObjectFree.
 */
JsonObjectH ngsMapGetSelectionStyle(unsigned char mapId, enum ngsStyleType styleType)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return nullptr;
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        outMessage(COD_GET_FAILED, _("MapView pointer is null"));
        return nullptr;
    }

    return new CPLJSONObject(mapView->selectionStyle(styleType));
}

int ngsMapSetSelectionsStyle(unsigned char mapId, enum ngsStyleType styleType,
                             JsonObjectH style)
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

const char *ngsMapGetSelectionStyleName(unsigned char mapId, ngsStyleType styleType)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED,  _("MapStore is not initialized"));
        return "";
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        outMessage(COD_GET_FAILED,  _("Failed to get mapview"));
        return "";
    }

    return storeCString(mapView->selectionStyleName(styleType));
}

int ngsMapSetSelectionStyleName(unsigned char mapId, enum ngsStyleType styleType,
                                const char *name)
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

int ngsMapIconSetAdd(unsigned char mapId, const char *name, const char *path,
                     char ownByMap)
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

int ngsMapIconSetRemove(unsigned char mapId, const char *name)
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

char ngsMapIconSetExists(unsigned char mapId, const char *name)
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
        outMessage(COD_GET_FAILED, _("Layer pointer is null"));
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
    static_cast<Layer*>(layer)->setVisible(visible);
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
        outMessage(COD_SET_FAILED, _("Layer pointer is null"));
        return nullptr;
    }

    return static_cast<Layer*>(layer)->datasource().get();
}

JsonObjectH ngsLayerGetStyle(LayerH layer)
{
    if(nullptr == layer) {
        outMessage(COD_SET_FAILED, _("Layer pointer is null"));
        return nullptr;
    }

    Layer *layerPtr = static_cast<Layer*>(layer);
    GlRenderLayer *renderLayerPtr = dynamic_cast<GlRenderLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        outMessage(COD_UNSUPPORTED, _("Layer type is unsupported. Mast be GlRenderLayer"));
        return nullptr;
    }

    if(!renderLayerPtr->style()){
        outMessage(COD_GET_FAILED, _("Style is not set"));
        return nullptr;
    }

    return new CPLJSONObject(renderLayerPtr->style()->save());
}

int ngsLayerSetStyle(LayerH layer, JsonObjectH style)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }

    Layer *layerPtr = static_cast<Layer*>(layer);
    GlRenderLayer *renderLayerPtr = dynamic_cast<GlRenderLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        outMessage(COD_UNSUPPORTED, _("Layer type is unsupported. Mast be GlRenderLayer"));
    }

    if(!renderLayerPtr->style()){
        outMessage(COD_GET_FAILED, _("Style is not set"));
    }

    CPLJSONObject *gdalJsonObject = static_cast<CPLJSONObject*>(style);
    renderLayerPtr->style()->load(*gdalJsonObject);

    return COD_SUCCESS;
}

const char *ngsLayerGetStyleName(LayerH layer)
{
    if(nullptr == layer) {
        outMessage(COD_SET_FAILED, _("Layer pointer is null"));
        return "";
    }

    Layer *layerPtr = static_cast<Layer*>(layer);
    GlRenderLayer *renderLayerPtr = dynamic_cast<GlRenderLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        outMessage(COD_UNSUPPORTED, _("Layer type is unsupported. Mast be GlRenderLayer"));
        return "";
    }

    if(!renderLayerPtr->style()){
        outMessage(COD_GET_FAILED, _("Style is not set"));
        return "";
    }

    return storeCString(renderLayerPtr->style()->name());
}

int ngsLayerSetStyleName(LayerH layer, const char *name)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }

    Layer *layerPtr = static_cast<Layer*>(layer);
    GlRenderLayer *renderLayerPtr = dynamic_cast<GlRenderLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        return outMessage(COD_UNSUPPORTED, _("Layer type is unsupported. Mast be GlRenderLayer"));
    }

    renderLayerPtr->setStyle(fromCString(name));
    return COD_SUCCESS;
}

int ngsLayerSetSelectionIds(LayerH layer, long long *ids, int size)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }
    Layer *layerPtr = static_cast<Layer*>(layer);
    GlSelectableFeatureLayer *renderLayerPtr =
            dynamic_cast<GlSelectableFeatureLayer*>(layerPtr);
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

int ngsLayerSetHideIds(LayerH layer, long long *ids, int size)
{
    if(nullptr == layer) {
        return outMessage(COD_SET_FAILED, _("Layer pointer is null"));
    }
    Layer *layerPtr = static_cast<Layer*>(layer);
    GlFeatureLayer *renderLayerPtr = dynamic_cast<GlFeatureLayer*>(layerPtr);
    if(nullptr == renderLayerPtr) {
        return outMessage(COD_UNSUPPORTED, _("Layer type is unsupported. Mast be GlFeatureLayer"));
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


static OverlayPtr getOverlay(unsigned char mapId, enum ngsMapOverlayType type)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        outMessage(COD_GET_FAILED, _("MapStore is not initialized"));
        return nullptr;
    }
    MapViewPtr mapView = mapStore->getMap(mapId);
    if(!mapView) {
        outMessage(COD_GET_FAILED, _("MapView pointer is null"));
        return nullptr;
    }
    return mapView->getOverlay(type);
}

template<typename T>
static T *getOverlay(unsigned char mapId, enum ngsMapOverlayType type)
{
    OverlayPtr overlay = getOverlay(mapId, type);
    if(!overlay) {
        outMessage(COD_GET_FAILED, _("Overlay pointer is null"));
        return nullptr;
    }
    return ngsDynamicCast(T, overlay);
}

int ngsOverlaySetVisible(unsigned char mapId, int typeMask, char visible)
{
    MapStore * const mapStore = MapStore::instance();
    if(nullptr == mapStore) {
        return outMessage(COD_DELETE_FAILED, _("MapStore is not initialized"));
    }
    return mapStore->setOverlayVisible(mapId, typeMask, visible) ?
                COD_SUCCESS : COD_SET_FAILED;
}

char ngsOverlayGetVisible(unsigned char mapId, enum ngsMapOverlayType type)
{
    OverlayPtr overlay = getOverlay(mapId, type);
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
int ngsOverlaySetOptions(unsigned char mapId, enum ngsMapOverlayType type,
                         char **options)
{
    OverlayPtr overlay = getOverlay(mapId, type);
    if(!overlay) {
        return COD_GET_FAILED;
    }
    Options editOptions(options);
    return overlay->setOptions(editOptions) ? COD_SUCCESS : COD_SET_FAILED;
}

char **ngsOverlayGetOptions(unsigned char mapId, enum ngsMapOverlayType type)
{
    OverlayPtr overlay = getOverlay(mapId, type);
    if(!overlay) {
        return nullptr;
    }
    Options options = overlay->options();
    if(options.empty()) {
        return nullptr;
    }
    return options.asCharArray().release();
}

ngsPointId ngsEditOverlayTouch(unsigned char mapId, double x, double y,
                               enum ngsMapTouchType type)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return {NOT_FOUND, 0};
    }
    return editOverlay->touch(x, y, type);
}

char ngsEditOverlayUndo(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return 0;
    }
    return editOverlay->undo();
}

char ngsEditOverlayRedo(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return 0;
    }
    return editOverlay->redo();
}

char ngsEditOverlayCanUndo(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return 0;
    }
    return editOverlay->canUndo();
}

char ngsEditOverlayCanRedo(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return 0;
    }
    return editOverlay->canRedo();
}

/**
 * @brief ngsEditOverlaySave Saves edits in feature class
 * @param mapId Map identificator the edit overlay belongs to
 * @return Feature handle or NULL if error occured
 */
FeatureH ngsEditOverlaySave(unsigned char mapId)
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

int ngsEditOverlayCancel(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_INVALID;
    }
    editOverlay->cancel();
    return COD_SUCCESS;
}

int ngsEditOverlayCreateGeometryInLayer(unsigned char mapId, LayerH layer,
                                        char empty)
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


int ngsEditOverlayCreateGeometry(unsigned char mapId, ngsGeometryType type)
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

int ngsEditOverlayEditGeometry(unsigned char mapId, LayerH layer,
                               long long featureId)
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

int ngsEditOverlayDeleteGeometry(unsigned char mapId)
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

int ngsEditOverlayAddPoint(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_INSERT_FAILED;
    }

    return editOverlay->createPoint() ? COD_SUCCESS : COD_INSERT_FAILED;
}

int ngsEditOverlayAddVertex(unsigned char mapId, ngsCoordinate coordinates)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return COD_INSERT_FAILED;
    }

    return editOverlay->addPoint(coordinates.X, coordinates.Y) ?
                COD_SUCCESS : COD_INSERT_FAILED;
}

enum ngsEditDeleteResult ngsEditOverlayDeletePoint(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return EDT_SELTYPE_NO_CHANGE;
    }
    return editOverlay->deletePoint();
}

int ngsEditOverlayAddHole(unsigned char mapId)
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

enum ngsEditDeleteResult ngsEditOverlayDeleteHole(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
       return EDT_SELTYPE_NO_CHANGE;
    }
    return editOverlay->deleteHole();
}

int ngsEditOverlayAddGeometryPart(unsigned char mapId)
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

void ngsEditOverlaySetWalkingMode(unsigned char mapId, char enable)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return;
    }
    editOverlay->setWalkingMode(enable == 1);
}

char ngsEditOverlayGetWalkingMode(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return 0;
    }
    return editOverlay->isWalkingMode() ? 1 : 0;
}

/**
 *
 * @param mapId
 * @return The value from enum ngsEditDeleteResult
 */
enum ngsEditDeleteResult ngsEditOverlayDeleteGeometryPart(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return EDT_SELTYPE_NO_CHANGE;
    }
    return editOverlay->deleteGeometryPart();
}

GeometryH ngsEditOverlayGetGeometry(unsigned char mapId)
{
    EditLayerOverlay *editOverlay = getOverlay<EditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == editOverlay) {
        return nullptr;
    }
    return editOverlay->geometry();
}

int ngsEditOverlaySetStyle(unsigned char mapId, enum ngsEditStyleType type,
                           JsonObjectH style)
{
    GlEditLayerOverlay *overlay = getOverlay<GlEditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == overlay) {
        return COD_DELETE_FAILED;
    }
    return overlay->setStyle(type, *static_cast<CPLJSONObject*>(style)) ?
                COD_SUCCESS : COD_SET_FAILED;
}

int ngsEditOverlaySetStyleName(unsigned char mapId, enum ngsEditStyleType type,
                               const char* name)
{
    GlEditLayerOverlay *overlay = getOverlay<GlEditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == overlay) {
        return COD_DELETE_FAILED;
    }
    return overlay->setStyleName(type, name) ? COD_SUCCESS : COD_SET_FAILED;
}

JsonObjectH ngsEditOverlayGetStyle(unsigned char mapId,
                                   enum ngsEditStyleType type)
{
    GlEditLayerOverlay *overlay = getOverlay<GlEditLayerOverlay>(mapId, MOT_EDIT);
    if(nullptr == overlay) {
        return nullptr;
    }
    return new CPLJSONObject(overlay->style(type));
}

int ngsLocationOverlayUpdate(unsigned char mapId, ngsCoordinate location,
                             float direction, float accuracy)
{
    LocationOverlay *overlay = getOverlay<LocationOverlay>(mapId, MOT_LOCATION);
    if(nullptr == overlay) {
        return COD_UPDATE_FAILED;
    }

    overlay->setLocation(location, direction, accuracy);
    return COD_SUCCESS;
}

int ngsLocationOverlaySetStyle(unsigned char mapId, JsonObjectH style)
{
    GlLocationOverlay *overlay = getOverlay<GlLocationOverlay>(mapId, MOT_LOCATION);
    if(nullptr == overlay) {
        return COD_UPDATE_FAILED;
    }

    return overlay->setStyle(*static_cast<CPLJSONObject*>(style)) ?
                COD_SUCCESS : COD_SET_FAILED;
}

int ngsLocationOverlaySetStyleName(unsigned char mapId, const char* name)
{
    GlLocationOverlay *overlay = getOverlay<GlLocationOverlay>(mapId, MOT_LOCATION);
    if(nullptr == overlay) {
        return COD_UPDATE_FAILED;
    }

    return overlay->setStyleName(name) ? COD_SUCCESS : COD_SET_FAILED;
}

JsonObjectH ngsLocationOverlayGetStyle(unsigned char mapId)
{
    GlLocationOverlay *overlay = getOverlay<GlLocationOverlay>(mapId, MOT_LOCATION);
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
