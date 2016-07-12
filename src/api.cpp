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
 
#include "api.h"
#include "version.h"
#include "datastore.h"
#include "mapstore.h"

// GDAL
#include "gdal.h"
#include "cpl_string.h"

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
#include "json-c/json_c_version.h"
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
        return NGM_VERSION_NUM;
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
        return NGM_VERSION_NUM;
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
        return NGM_VERSION;
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
        if(nullptr == gDataStore)
            gDataStore.reset (new DataStore(nullptr, nullptr, nullptr));
        return gDataStore->formats ().c_str();
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
        return NGM_VERSION;
    return nullptr;
}

/**
 * @brief init library structures
 * @param path path to data store folder
 * @param cachePath path to cache folder
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsInit(const char* path, const char* dataPath, const char* cachePath)
{
    gDataStore.reset(new DataStore(path, dataPath, cachePath));
    int nResult = gDataStore->open();
    bool create = false;
    if(nResult != ngsErrorCodes::SUCCESS){
        nResult = gDataStore->create ();
        create = true;
    }

    if(nResult != ngsErrorCodes::SUCCESS) {
        gDataStore.reset();
    }
    else {
        gMapStore.reset (new MapStore(gDataStore));
        if(create)
            nResult = gMapStore->create();
    }
    return nResult;
}

/**
 * @brief Clean up library structures
 */
void ngsUninit()
{
    gMapStore.reset ();
    gDataStore.reset ();
}

/**
 * @brief Delete storage directory and cache (optional)
 * @param path Path to storage
 * @param cachePath Path to cache (may be NULL)
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsDestroy(const char *path, const char *cachePath)
{
    if(nullptr == path)
        return ngsErrorCodes::INVALID_PATH;
    gMapStore.reset ();
    gDataStore.reset(new DataStore(path, nullptr, cachePath));

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

    return gDataStore->createRemoteTMSRaster(url, name, alias, copyright, epsg,
                                             z_min, z_max, y_origin_top);
}

/**
 * @brief Load raster file to storage. Maybe georectify raster in 3857 spatial
 *        reference or NGM v.1 and v.2 zip with tiles
 * @param path Path to file in OS
 * @param name Layer name, only alpha, numeric and underline
 * @param alias Layer alias name. User readable text
 * @param move Move data to storage or copy
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsLoadRaster(const char */*path*/, const char */*name*/,
                  const char */*alias*/, bool /*move*/)
{
    return ngsErrorCodes::SUCCESS;
}

/**
 * @brief Inititialise map with buffer and it size in pixels
 * @param name Map name
 * @param buffer Pointer to buffer. Size should be enouth to store image data width x height
 * @param width Output image width
 * @param height Output image height
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsInitMap(const char *name, void *buffer, int width, int height)
{
    if(nullptr == gMapStore)
        return ngsErrorCodes::INIT_FAILED;
    return gMapStore->initMap (name, buffer, width, height);
}

/**
 * @brief Inform library if low memory event occures
 */
void ngsOnLowMemory()
{
    if(nullptr != gMapStore)
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
    if(nullptr != gMapStore)
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
 * @param name Map name
 * @param callback Progress function
 * @param callbackData Progress function arguments
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsDrawMap(const char *name, ngsProgressFunc callback, void* callbackData)
{
    if(nullptr == gMapStore)
        return ngsErrorCodes::INIT_FAILED;
    return gMapStore->drawMap (name, callback, callbackData);
}

/**
 * @brief ngsGetMapBackgroundColor Map background color
 * @param name Map name
 * @return map background color struct
 */
ngsRGBA ngsGetMapBackgroundColor(const char *name)
{
    if(nullptr == gMapStore)
        return {0,0,0,0};
    return gMapStore->getMapBackgroundColor (name);

}

/**
 * @brief ngsSetMapBackgroundColor set specified by name map background color
 * @param name Map name
 * @param R red
 * @param G green
 * @param B blue
 * @param A alpha
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsSetMapBackgroundColor(const char *name, unsigned char R, unsigned char G,
                             unsigned char B, unsigned char A)
{
    if(nullptr == gMapStore)
        return ngsErrorCodes::INIT_FAILED;
    return gMapStore->setMapBackgroundColor (name, {R, G, B, A});

}
