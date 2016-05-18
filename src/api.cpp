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

#include "gdal.h"
#include <curl/curl.h>
#include "geos_c.h"
#include "sqlite3.h"

#include <memory>

using namespace ngs;
using namespace std;

static unique_ptr<DataStore> gDataStore;
static unique_ptr<MapStore> gMapStore;

 
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

    if(EQUAL(request, "gdal"))
        return atoi(GDALVersionInfo("VERSION_NUM"));
    else if(EQUAL(request, "curl")){
        curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
        return data->version_num;
    }
    else if(EQUAL(request, "geos"))
        return GEOS_CAPI_LAST_INTERFACE;
    else if(EQUAL(request, "sqlite"))
        return sqlite3_libversion_number();
    //else if(request == nullptr || EQUAL(request, "self"))
    return NGM_VERSION_NUM;
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

    if(EQUAL(request, "gdal"))
        return GDALVersionInfo("RELEASE_NAME");
    else if(EQUAL(request, "curl")){
        curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
        return data->version;
    }
    else if(EQUAL(request, "geos"))
        return GEOS_CAPI_VERSION;
    else if(EQUAL(request, "sqlite"))
        return sqlite3_libversion();
    return NGM_VERSION;
}

/**
 * @brief init library structures
 * @param path path to data store folder
 * @param cachePath path to cache folder
 * @return ngsErrorCodes value - SUCCES if everything is OK
 */
int ngsInit(const char* path, const char* cachePath)
{
    gDataStore.reset(new DataStore(path, cachePath));
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
}
