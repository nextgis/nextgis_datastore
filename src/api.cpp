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

#include <curl/curl.h>
#include "geos_c.h"
#include "sqlite3.h"
#include "json-c/json_c_version.h"
#include "proj_api.h"
#include "jpeglib.h"
#include "tiff.h"
#include "geotiff.h"
#include "png.h"
#include "expat.h"
#include "iconv.h"
#include "zlib.h"
#include "openssl/opensslv.h"

#include <memory>
#include <iostream>

using namespace ngs;
using namespace std;

static unique_ptr<DataStore> gDataStore;
static unique_ptr<MapStore> gMapStore;

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
    else if(EQUAL(request, "curl")){
        curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
        return static_cast<int>(data->version_num);
    }
    else if(EQUAL(request, "geos"))
        return GEOS_CAPI_LAST_INTERFACE;
    else if(EQUAL(request, "sqlite"))
        return sqlite3_libversion_number();    
    else if(EQUAL(request, "jsonc"))
        return JSON_C_VERSION_NUM;
    else if(EQUAL(request, "proj"))
        return PJ_VERSION;
    else if(EQUAL(request, "jpeg"))
        return JPEG_LIB_VERSION;
    else if(EQUAL(request, "tiff"))
        return TIFF_VERSION_BIG;
    else if(EQUAL(request, "geotiff"))
        return LIBGEOTIFF_VERSION;
    else if(EQUAL(request, "png"))
        return PNG_LIBPNG_VER;
    else if(EQUAL(request, "expat"))
        return XML_MAJOR_VERSION * 100 + XML_MINOR_VERSION * 10 + XML_MICRO_VERSION;
    else if(EQUAL(request, "iconv"))
        return _LIBICONV_VERSION;
    else if(EQUAL(request, "zlib"))
        return ZLIB_VERNUM;
    else if(EQUAL(request, "openssl"))
        return OPENSSL_VERSION_NUMBER;
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
    else if(EQUAL(request, "curl")){
        curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
        return data->version;
    }
    else if(EQUAL(request, "geos"))
        return GEOS_CAPI_VERSION;
    else if(EQUAL(request, "sqlite"))
        return sqlite3_libversion();
    else if(EQUAL(request, "formats")){
        if(nullptr == gDataStore)
            gDataStore.reset (new DataStore(nullptr, nullptr, nullptr));
        return gDataStore->formats ().c_str();
    }
    else if(EQUAL(request, "jsonc"))
        return JSON_C_VERSION;
    else if(EQUAL(request, "proj")) {
        int major = PJ_VERSION / 100;
        int major_full = major * 100;
        int minor = (PJ_VERSION - major_full) / 10;
        int minor_full = minor * 10;
        int rev = PJ_VERSION - (major_full + minor_full);
        return CPLSPrintf("%d.%d.%d", major, minor, rev);
    }
    else if(EQUAL(request, "jpeg"))
        return CPLSPrintf("%d.%d", JPEG_LIB_VERSION_MAJOR, JPEG_LIB_VERSION_MINOR);
    else if(EQUAL(request, "tiff")){
        int major = TIFF_VERSION_BIG / 10;
        int major_full = major * 10;
        int minor = TIFF_VERSION_BIG - major_full;
        return CPLSPrintf("%d.%d", major, minor);
    }
    else if(EQUAL(request, "geotiff")) {
        int major = LIBGEOTIFF_VERSION / 1000;
        int major_full = major * 1000;
        int minor = (LIBGEOTIFF_VERSION - major_full) / 100;
        int minor_full = minor * 100;
        int rev = (LIBGEOTIFF_VERSION - (major_full + minor_full)) / 10;
        return CPLSPrintf("%d.%d.%d", major, minor, rev);
    }
    else if(EQUAL(request, "png"))
        return PNG_LIBPNG_VER_STRING;
    else if(EQUAL(request, "expat"))
        return CPLSPrintf("%d.%d.%d", XML_MAJOR_VERSION, XML_MINOR_VERSION,
                          XML_MICRO_VERSION);
    else if(EQUAL(request, "iconv")) {
        int major = _LIBICONV_VERSION / 256;
        int minor = _LIBICONV_VERSION - major * 256;
        return CPLSPrintf("%d.%d", major, minor);
    }
    else if(EQUAL(request, "zlib"))
        return ZLIB_VERSION;
    else if(EQUAL(request, "openssl")) {
        string val = CPLSPrintf("%#lx", OPENSSL_VERSION_NUMBER);
        int major = atoi(val.substr (2, 1).c_str ());
        int minor = atoi(val.substr (3, 2).c_str ());
        int rev = atoi(val.substr (5, 2).c_str ());
        return CPLSPrintf("%d.%d.%d", major, minor, rev);
    }
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
    int nResult = gDataStore->openOrCreate();
    if(nResult != ngsErrorCodes::SUCCESS)
        gDataStore.reset(nullptr);
    return nResult;
}

/**
 * @brief Clean up library structures
 */
void ngsUninit()
{
    gDataStore.reset(nullptr);
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
int ngsLoadRaster(const char */*path*/, const char */*name*/, const char */*alias*/, bool /*move*/)
{
    return ngsErrorCodes::SUCCESS;
}
