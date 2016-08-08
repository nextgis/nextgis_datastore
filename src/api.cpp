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
 
#include "api_priv.h"
#include "datastore.h"
#include "version.h"
#include "mapstore.h"
#include "constants.h"

// GDAL
#include "gdal.h"
#include "cpl_string.h"
#include "cpl_csv.h"

#ifdef HAVE_CURL_H
#include <curl/curlver.h>
#endif

#ifdef HAVE_GEOS_C_H
#include "geos_c.h"
#endif

#ifdef HAVE_SQLITE3_H
#include "sqlite3.h"
#endif

#ifdef HAVE_JSON_C_VERSION_H
#include "json_c_version.h"
#endif

#ifdef HAVE_PROJ_API_H
#include "proj_api.h"
#endif

#ifdef HAVE_JPEGLIB_H
#include "jpeglib.h"
#endif

#ifdef HAVE_TIFF_H
#include "tiff.h"
#endif

#ifdef HAVE_GEOTIFF_H
#include "geotiff.h"
#endif

#ifdef HAVE_PNG_H
#include "png.h"
#endif

#ifdef HAVE_EXPAT_H
#include "expat.h"
#endif

#ifdef HAVE_ICONV_H
#include "iconv.h"
#endif

#ifdef HAVE_ZLIB_H
#include "zlib.h"
#endif

#ifdef HAVE_OPENSSLV_H
#include "openssl/opensslv.h"
#endif


#include <memory>
#include <iostream>

using namespace ngs;
using namespace std;

static DataStorePtr gDataStore;
static MapStorePtr gMapStore;
static string gFormats;

/**
 * @brief ngsInitDataStore Open or create data store. All datastore functgions
 * will work with this datastore object until new one willn ot open.
 * @param path Path to create datastore (geopackage database name)
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsInitDataStore(const char *path)
{
    if(gDataStore && gDataStore->path().compare (path) == 0)
        return ngsErrorCodes::SUCCESS;
    gDataStore = DataStore::openOrCreate(path);
    if(nullptr == gDataStore)
        return ngsErrorCodes::OPEN_FAILED;
    return ngsErrorCodes::SUCCESS;
}

void initMapStore()
{
    if(nullptr == gMapStore){
        gMapStore.reset (new MapStore());
    }
}

DataStorePtr ngsGetCurrentDataStore()
{
    return gDataStore;
}

void ngsSetCurrentDataStore(ngs::DataStorePtr ds)
{
    gDataStore = ds;
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
    if(cachePath)
        CPLSetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", cachePath);

#ifdef _DEBUG
    cout << "HTTP user agent set to: " << NGS_USERAGENT << endl;
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
 * @brief reportFormats Return supported GDAL formats. Note, that GDAL library
 * must be initialized before execute function. In other case empty string will
 * return.
 * @return supported raster/vector/network formats or empty string
 */
string reportFormats()
{
    for( int iDr = 0; iDr < GDALGetDriverCount(); iDr++ ) {
        GDALDriverH hDriver = GDALGetDriver(iDr);

        const char *pszRFlag = "", *pszWFlag, *pszVirtualIO, *pszSubdatasets,
                *pszKind;
        char** papszMD = GDALGetMetadata( hDriver, NULL );

        if( CSLFetchBoolean( papszMD, GDAL_DCAP_OPEN, FALSE ) )
            pszRFlag = "r";

        if( CSLFetchBoolean( papszMD, GDAL_DCAP_CREATE, FALSE ) )
            pszWFlag = "w+";
        else if( CSLFetchBoolean( papszMD, GDAL_DCAP_CREATECOPY, FALSE ) )
            pszWFlag = "w";
        else
            pszWFlag = "o";

        if( CSLFetchBoolean( papszMD, GDAL_DCAP_VIRTUALIO, FALSE ) )
            pszVirtualIO = "v";
        else
            pszVirtualIO = "";

        if( CSLFetchBoolean( papszMD, GDAL_DMD_SUBDATASETS, FALSE ) )
            pszSubdatasets = "s";
        else
            pszSubdatasets = "";

        if( CSLFetchBoolean( papszMD, GDAL_DCAP_RASTER, FALSE ) &&
            CSLFetchBoolean( papszMD, GDAL_DCAP_VECTOR, FALSE ))
            pszKind = "raster,vector";
        else if( CSLFetchBoolean( papszMD, GDAL_DCAP_RASTER, FALSE ) )
            pszKind = "raster";
        else if( CSLFetchBoolean( papszMD, GDAL_DCAP_VECTOR, FALSE ) )
            pszKind = "vector";
        else if( CSLFetchBoolean( papszMD, GDAL_DCAP_GNM, FALSE ) )
            pszKind = "geography network";
        else
            pszKind = "unknown kind";

        gFormats += CPLSPrintf( "  %s -%s- (%s%s%s%s): %s\n",
                GDALGetDriverShortName( hDriver ),
                pszKind,
                pszRFlag, pszWFlag, pszVirtualIO, pszSubdatasets,
                GDALGetDriverLongName( hDriver ) );
    }

    return gFormats;
}


#ifndef _LIBICONV_VERSION
#define _LIBICONV_VERSION 0x010E
#endif

/**
 * @brief Get library version number as major * 10000 + minor * 100 + rev
 * @param request may be gdal, proj, geos, curl, jpeg, png, zlib, iconv, sqlite3,
 *        openssl, expat, jsonc, tiff, geotiff
 * @return library version number
 */
int ngsGetVersion(const char* request)
{
    if(nullptr == request)
        return NGS_VERSION_NUM;
    else if(EQUAL(request, "gdal"))
        return atoi(GDALVersionInfo("VERSION_NUM"));
#ifdef HAVE_CURL_H
    else if(EQUAL(request, "curl"))
        return LIBCURL_VERSION_NUM;
#endif
#ifdef HAVE_GEOS_C_H
    else if(EQUAL(request, "geos"))
        return GEOS_CAPI_LAST_INTERFACE;
#endif
#ifdef HAVE_SQLITE3_H
    else if(EQUAL(request, "sqlite"))
        // TODO: check if GDAL compiled with sqlite
        return sqlite3_libversion_number();    
#endif
#ifdef HAVE_JSON_C_H
    else if(EQUAL(request, "jsonc"))
        return JSON_C_VERSION_NUM;
#endif
#ifdef HAVE_PROJ_API_H
    else if(EQUAL(request, "proj"))
        return PJ_VERSION;
#endif
#ifdef HAVE_JPEGLIB_H
    else if(EQUAL(request, "jpeg"))
        return JPEG_LIB_VERSION;
#endif
#ifdef HAVE_TIFF_H
    else if(EQUAL(request, "tiff"))
        return TIFF_VERSION_BIG;
#endif
#ifdef HAVE_GEOTIFF_H
    else if(EQUAL(request, "geotiff"))
        return LIBGEOTIFF_VERSION;
#endif
#ifdef HAVE_PNG_H
    else if(EQUAL(request, "png"))
        return PNG_LIBPNG_VER;
#endif
#ifdef HAVE_EXPAT_H
    else if(EQUAL(request, "expat"))
        return XML_MAJOR_VERSION * 100 + XML_MINOR_VERSION * 10 + XML_MICRO_VERSION;
#endif
#ifdef HAVE_ICONV_H
    else if(EQUAL(request, "iconv"))
        return _LIBICONV_VERSION;
#endif
#ifdef HAVE_ZLIB_H
    else if(EQUAL(request, "zlib"))
        return ZLIB_VERNUM;
#endif
#ifdef HAVE_OPENSSLV_H
    else if(EQUAL(request, "openssl"))
        return OPENSSL_VERSION_NUMBER;
#endif
    else if(EQUAL(request, "self"))
        return NGS_VERSION_NUM;
    return 0;
}

/**
 * @brief Get library version string
 * @param request may be gdal, proj, geos, curl, jpeg, png, zlib, iconv, sqlite3,
 *        openssl, expat, jsonc, tiff, geotiff
 * @return library version string
 */
const char* ngsGetVersionString(const char* request)
{
    if(nullptr == request)
        return NGS_VERSION;
    else if(EQUAL(request, "gdal"))
        return GDALVersionInfo("RELEASE_NAME");
#ifdef HAVE_CURL_H
    else if(EQUAL(request, "curl"))
        return LIBCURL_VERSION;
#endif
#ifdef HAVE_GEOS_C_H
    else if(EQUAL(request, "geos"))
        return GEOS_CAPI_VERSION;
#endif
#ifdef HAVE_SQLITE3_H
    else if(EQUAL(request, "sqlite"))
        // TODO: check if GDAL compiled with sqlite
        return sqlite3_libversion();
#endif
    else if(EQUAL(request, "formats")){
        return reportFormats().c_str ();
    }
#ifdef HAVE_JSON_C_H
    else if(EQUAL(request, "jsonc"))
        return JSON_C_VERSION;
#endif
#ifdef HAVE_PROJ_API_H
    else if(EQUAL(request, "proj")) {
        int major = PJ_VERSION / 100;
        int major_full = major * 100;
        int minor = (PJ_VERSION - major_full) / 10;
        int minor_full = minor * 10;
        int rev = PJ_VERSION - (major_full + minor_full);
        return CPLSPrintf("%d.%d.%d", major, minor, rev);
    }
#endif
#ifdef HAVE_JPEGLIB_H
    else if(EQUAL(request, "jpeg"))
#if JPEG_LIB_VERSION >= 90
        return CPLSPrintf("%d.%d", JPEG_LIB_VERSION_MAJOR, JPEG_LIB_VERSION_MINOR);
#else
        return CPLSPrintf("%d.0", JPEG_LIB_VERSION / 10);
#endif
#endif
#ifdef HAVE_TIFF_H
    else if(EQUAL(request, "tiff")){
        int major = TIFF_VERSION_BIG / 10;
        int major_full = major * 10;
        int minor = TIFF_VERSION_BIG - major_full;
        return CPLSPrintf("%d.%d", major, minor);
    }
#endif
#ifdef HAVE_GEOTIFF_H
    else if(EQUAL(request, "geotiff")) {
        int major = LIBGEOTIFF_VERSION / 1000;
        int major_full = major * 1000;
        int minor = (LIBGEOTIFF_VERSION - major_full) / 100;
        int minor_full = minor * 100;
        int rev = (LIBGEOTIFF_VERSION - (major_full + minor_full)) / 10;
        return CPLSPrintf("%d.%d.%d", major, minor, rev);
    }
#endif
#ifdef HAVE_PNG_H
    else if(EQUAL(request, "png"))
        return PNG_LIBPNG_VER_STRING;
#endif
#ifdef HAVE_EXPAT_H
    else if(EQUAL(request, "expat"))
        return CPLSPrintf("%d.%d.%d", XML_MAJOR_VERSION, XML_MINOR_VERSION,
                          XML_MICRO_VERSION);
#endif
#ifdef HAVE_ICONV_H
    else if(EQUAL(request, "iconv")) {
        int major = _LIBICONV_VERSION / 256;
        int minor = _LIBICONV_VERSION - major * 256;
        return CPLSPrintf("%d.%d", major, minor);
    }
#endif
#ifdef HAVE_ZLIB_H
    else if(EQUAL(request, "zlib"))
        return ZLIB_VERSION;
#endif
#ifdef HAVE_OPENSSLV_H
    else if(EQUAL(request, "openssl")) {
        string val = CPLSPrintf("%#lx", OPENSSL_VERSION_NUMBER);
        int major = atoi(val.substr (2, 1).c_str ());
        int minor = atoi(val.substr (3, 2).c_str ());
        int rev = atoi(val.substr (5, 2).c_str ());
        return CPLSPrintf("%d.%d.%d", major, minor, rev);
    }
#endif
    else if(EQUAL(request, "self"))
        return NGS_VERSION;
    return nullptr;
}

/**
 * @brief init library structures
 * @param dataPath path to GDAL_DATA folder
 * @param cachePath path to cache folder
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsInit(const char* dataPath, const char* cachePath)
{
#ifdef NGS_MOBILE
    if(nullptr == dataPath)
        return ngsErrorCodes::PATH_NOT_SPECIFIED;
#endif

    initGDAL(dataPath, cachePath);

    return ngsErrorCodes::SUCCESS;
}

/**
 * @brief Clean up library structures
 */
void ngsUninit()
{
    gMapStore.reset ();
    gDataStore.reset ();

    GDALDestroyDriverManager();
}

/**
 * @brief Delete storage directory and cache (optional)
 * @param path Path to storage
 * @param cachePath Path to cache (may be NULL)
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsDestroyDataStore(const char *path, const char *cachePath)
{
    if(nullptr == path)
        return ngsErrorCodes::INVALID;
    gMapStore.reset ();
    if(cachePath){
        if(CPLUnlinkTree(cachePath) != 0){
            return ngsErrorCodes::DELETE_FAILED;
        }
    }
    ngsInitDataStore(path);
    return gDataStore->destroy ();
}

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
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsCreateRemoteTMSRaster(const char *url, const char *name, const char *alias,
                             const char *copyright, int epsg, int z_min, int z_max,
                             bool y_origin_top)
{
    if(z_max == 0)
        z_max = 18;

    if(gDataStore)
        return gDataStore->createRemoteTMSRaster(url, name, alias, copyright,
                                            epsg, z_min, z_max, y_origin_top);

    return ngsErrorCodes::CREATE_FAILED;
}

/**
 * @brief Inititialise map with buffer and it size in pixels
 * @param mapId Map id received from create or open map functions
 * @param buffer Pointer to buffer. Size should be enouth to store image data width x height
 * @param width Output image width
 * @param height Output image height
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsInitMap(unsigned int mapId, void *buffer, int width, int height, int isYAxisInverted)
{
    initMapStore();
    return gMapStore->initMap (mapId, buffer, width, height, isYAxisInverted);
}

/**
 * @brief Inform library if low memory event occures
 */
void ngsOnLowMemory()
{
    initMapStore();
    gMapStore->onLowMemory ();
    if(nullptr != gDataStore)
        gDataStore->onLowMemory ();
}

/**
 * @brief Set notify function executed on some library events
 * @param Callback function pointer (not free by library)
 */
void ngsSetNotifyFunction(ngsNotifyFunc callback)
{
    if(nullptr != gDataStore)
        gDataStore->setNotifyFunc (callback);
    initMapStore();
    gMapStore->setNotifyFunc (callback);
}

/**
 * @brief Inform library to free resources as possible
 */
void ngsOnPause()
{
    // just free maps
    if(nullptr != gMapStore)
        gMapStore->onLowMemory ();
}

/**
 * @brief ngsDrawMap Start drawing map in specified (in ngsInitMap) extent
 * @param mapId Map id received from create or open map functions
 * @param callback Progress function
 * @param callbackData Progress function arguments
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsDrawMap(unsigned int mapId, ngsProgressFunc callback, void* callbackData)
{
    initMapStore();
    return gMapStore->drawMap (mapId, callback, callbackData);
}

/**
 * @brief ngsGetMapBackgroundColor Map background color
 * @param mapId Map id received from create or open map functions
 * @return map background color struct
 */
ngsRGBA ngsGetMapBackgroundColor(unsigned int mapId)
{
    initMapStore();
    return gMapStore->getMapBackgroundColor (mapId);
}

/**
 * @brief ngsSetMapBackgroundColor set specified by name map background color
 * @param mapId Map id received from create or open map functions
 * @param R red
 * @param G green
 * @param B blue
 * @param A alpha
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsSetMapBackgroundColor(unsigned int mapId, unsigned char R, unsigned char G,
                             unsigned char B, unsigned char A)
{
    initMapStore();
    return gMapStore->setMapBackgroundColor (mapId, {R, G, B, A});
}


int ngsSetMapDisplayCenter(unsigned int mapId, int x, int y)
{
    initMapStore();
    return gMapStore->setMapDisplayCenter(mapId, x, y);
}

int ngsGetMapDisplayCenter(unsigned int mapId, int* x, int* y)
{
    initMapStore();
    return gMapStore->getMapDisplayCenter(mapId, *x, *y);
}

int ngsSetMapScale(unsigned int mapId, double scale)
{
    initMapStore();
    return gMapStore->setMapScale(mapId, scale);
}

int ngsGetMapScale(unsigned int mapId, double* scale)
{
    initMapStore();
    return gMapStore->getMapScale(mapId, *scale);
}

/**
 * @brief ngsCreateMap Create new empty map
 * @param name Map name
 * @param description Map description
 * @param epsg EPSG code
 * @param minX minimum X coordinate
 * @param minY minimum Y coordinate
 * @param maxX maximum X coordinate
 * @param maxY maximum Y coordinate
 * @return -1 if create failed or map id.
 */
int ngsCreateMap(const char* name, const char* description,
                 unsigned short epsg, double minX, double minY,
                 double maxX, double maxY)
{
    initMapStore();
    return gMapStore->createMap (name, description, epsg, minX, minY, maxX, maxY);
}

/**
 * @brief ngsOpenMap Open existing map from file
 * @param path Path to map file
 * @return -1 if open failed or map id.
 */
int ngsOpenMap(const char *path)
{
    initMapStore();
    return gMapStore->openMap (path);
}

/**
 * @brief ngsSaveMap Save map to file
 * @param mapId Map id to save
 * @param path Path to store map data
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsSaveMap(unsigned int mapId, const char *path)
{
    initMapStore();
    return gMapStore->saveMap (mapId, path);
}

/**
 * @brief ngsLoad
 * @param name Destination dataset name
 * @param path Path to file in OS
 * @param subDatasetName Layer name, if datasource on path has several layers
 * @param move Move data to storage or copy
 * @param skipFlags combination of EMPTY_GEOMETRY(1) - to skip empty geometry
 * features to load, INVALID_GEOMETRY(2) - to skip invalid geometry or
 * NONE(0) - to load everything
 * @param callback Progress function
 * @param callbackData Progress function arguments
 * @return  ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsLoad(const char* name, const char *path, const char *subDatasetName,
            bool move, unsigned int skipFlags, ngsProgressFunc callback,
            void *callbackData)
{
    if(nullptr != gDataStore)
        return gDataStore->loadDataset (name, path, subDatasetName, move,
                                        skipFlags, callback, callbackData);
    return ngsErrorCodes::INSERT_FAILED;
}

int ngsCreateLayer(unsigned int mapId, const char *name, const char *path)
{
    ngsInitDataStore ( CPLGetDirname (path) );
    DatasetPtr dataset = gDataStore->getDataset ( CPLGetBasename (path) );
    initMapStore();
    MapPtr map = gMapStore->getMap (mapId);
    if(nullptr == map || nullptr == dataset)
        return ngsErrorCodes::CREATE_FAILED;
    return map->createLayer (name, dataset);

}
