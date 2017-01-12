/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2017 NextGIS, <info@nextgis.com>
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

#include "versionutil.h"
#include "version.h"

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

#ifndef _LIBICONV_VERSION
#define _LIBICONV_VERSION 0x010E
#endif

static CPLString gFormats;

/**
 * @brief reportFormats Return supported GDAL formats. Note, that GDAL library
 * must be initialized before execute function. In other case empty string will
 * return.
 * @return supported raster/vector/network formats or empty string
 */
std::string ngs::reportFormats()
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

int ngs::getVersion(const char* request)
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
        return SQLITE_VERSION_NUMBER;
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

const char* ngs::getVersionString(const char* request)
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
        return SQLITE_VERSION;
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
