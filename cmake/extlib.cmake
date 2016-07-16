################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public License v3
#
#  Copyright (c) 2016 NextGIS, <info@nextgis.com>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Lesser General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.
################################################################################

if(NOT BUILD_TARGET_PLATFORM STREQUAL "Desktop")
    set(WITH_ZLIB ON CACHE BOOL "zlib on" FORCE)

    set(WITH_SQLite3 ON CACHE BOOL "SQLite3 on" FORCE)
    set(WITH_OpenSSL ON CACHE BOOL "OpenSSL on" FORCE)
    set(WITH_ICONV ON CACHE BOOL "iconv on")
    set(WITH_CURL ON CACHE BOOL "CURL on" FORCE)
    set(WITH_GEOS ON CACHE BOOL "GEOS on" FORCE)
    set(WITH_PROJ4 ON CACHE BOOL "PROJ4 on" FORCE)
    set(WITH_EXPAT ON CACHE BOOL "EXPAT on" FORCE)
    set(WITH_JSONC ON CACHE BOOL "JSONC on" FORCE)
    set(WITH_TIFF ON CACHE BOOL "TIFF on" FORCE)
    set(WITH_GeoTIFF ON CACHE BOOL "GeoTIFF on" FORCE)
    set(WITH_JPEG ON CACHE BOOL "JPEG on" FORCE)
    set(WITH_PNG ON CACHE BOOL "JPEG on" FORCE)

    set(WITH_GDAL ON CACHE BOOL "GDAL on" FORCE)

    set(WITH_SQLite3_EXTERNAL ON CACHE BOOL "SQLite3 external on" FORCE)
    set(WITH_OpenSSL_EXTERNAL ON CACHE BOOL "OpenSSL external on" FORCE)
    set(WITH_ICONV_EXTERNAL ON CACHE BOOL "iconv external on")
    set(WITH_CURL_EXTERNAL ON CACHE BOOL "CURL external on" FORCE)
    set(WITH_GEOS_EXTERNAL ON CACHE BOOL "GEOS external on" FORCE)
    set(WITH_PROJ4_EXTERNAL ON CACHE BOOL "PROJ4 external on" FORCE)
    set(WITH_EXPAT_EXTERNAL ON CACHE BOOL "EXPAT external on" FORCE)
    set(WITH_JSONC_EXTERNAL ON CACHE BOOL "JSONC external on" FORCE)
    set(WITH_TIFF_EXTERNAL ON CACHE BOOL "TIFF external on" FORCE)
    set(WITH_GeoTIFF_EXTERNAL ON CACHE BOOL "GeoTIFF external on" FORCE)
    set(WITH_JPEG_EXTERNAL ON CACHE BOOL "JPEG external on" FORCE)
    set(WITH_PNG_EXTERNAL ON CACHE BOOL "JPEG external on" FORCE)
    set(WITH_GDAL_EXTERNAL ON CACHE BOOL "GDAL external on" FORCE)    


    set(WITH_JPEG12 OFF CACHE BOOL "JPEG12 off" FORCE)
    set(WITH_JPEG12_EXTERNAL OFF CACHE BOOL "JPEG12 external off" FORCE)
    set(WITH_LibLZMA OFF CACHE BOOL "LibLZMA off" FORCE)
    set(WITH_LibLZMA_EXTERNAL OFF CACHE BOOL "LibLZMA external off" FORCE)
    set(WITH_PostgreSQL OFF CACHE BOOL "PostgreSQL off" FORCE)
    set(WITH_PostgreSQL_EXTERNAL OFF CACHE BOOL "PostgreSQL external off" FORCE)
    set(WITH_LibXml2 OFF CACHE BOOL "LibXml2 off" FORCE)
    set(WITH_LibXml2_EXTERNAL OFF CACHE BOOL "LibXml2 external off" FORCE)

    add_definitions (-DHAVE_CURLVER_H)
    add_definitions (-DHAVE_GEOS_C_H)
    add_definitions (-DHAVE_SQLITE3_H)
    add_definitions (-DHAVE_JSON_C_VERSION_H)
    add_definitions (-DHAVE_PROJ_API_H)
    add_definitions (-DHAVE_JPEGLIB_H)
    add_definitions (-DHAVE_TIFF_H)
    add_definitions (-DHAVE_GEOTIFF_H)
    add_definitions (-DHAVE_PNG_H)
    add_definitions (-DHAVE_EXPAT_H)
    add_definitions (-DHAVE_ICONV_H)
    add_definitions (-DHAVE_ZLIB_H)
    add_definitions (-DHAVE_OPENSSLV_H)
endif()

find_anyproject(GDAL REQUIRED VERSION 2.0 SHARED OFF CMAKE_ARGS
    -DENABLE_PLSCENES=OFF
    -DENABLE_AAIGRID_GRASSASCIIGRID=OFF
    -DENABLE_ADRG_SRP=OFF
    -DENABLE_AIG=OFF
    -DENABLE_AIRSAR=OFF
    -DENABLE_ARG=OFF
    -DENABLE_BLX=OFF
    -DENABLE_BMP=OFF
    -DENABLE_BSB=OFF
    -DENABLE_CALS=OFF
    -DENABLE_CEOS=OFF
    -DENABLE_CEOS2=OFF
    -DENABLE_COASP=OFF
    -DENABLE_COSAR=OFF
    -DENABLE_CTG=OFF
    -DENABLE_DIMAP=OFF
    -DENABLE_DTED=OFF
    -DENABLE_E00GRID=OFF
    -DENABLE_ELAS=OFF
    -DENABLE_ENVISAT=OFF
    -DENABLE_ERS=OFF
    -DENABLE_FIT=OFF
    -DENABLE_GFF=OFF
    -DENABLE_GRIB=OFF
    -DENABLE_GSAG_GSBG_GS7BG=OFF
    -DENABLE_GXF=OFF
    -DENABLE_HF2=OFF
    -DENABLE_IDRISI_RASTER=OFF
    -DENABLE_ILWIS=OFF
    -DENABLE_INGR=OFF
    -DENABLE_IRIS=OFF
    -DENABLE_JAXAPALSAR=OFF
    -DENABLE_JDEM=OFF
    -DENABLE_KMLSUPEROVERLAY=OFF
    -DENABLE_L1B=OFF
    -DENABLE_LEVELLER=OFF
    -DENABLE_MAP=OFF
    -DENABLE_MBTILES=OFF
    -DENABLE_MSGN=OFF
    -DENABLE_NGSGEOID=OFF
    -DENABLE_NITF_RPFTOC_ECRGTOC=OFF
    -DENABLE_NWT=OFF
    -DENABLE_OZI=OFF
    -DENABLE_PDS_ISIS2_ISIS3_VICAR=OFF
    -DENABLE_PLMOSAIC=OFF
#    -DENABLE_PNG=OFF
    -DENABLE_POSTGISRASTER=OFF
    -DENABLE_R=OFF
    -DENABLE_RASTERLITE=OFF
    -DENABLE_RIK=OFF
    -DENABLE_RMF=OFF
    -DENABLE_RS2=OFF
    -DENABLE_SAGA=OFF
    -DENABLE_SDTS_RASTER=OFF
    -DENABLE_SGI=OFF
    -DENABLE_SRTMHGT=OFF
    -DENABLE_TERRAGEN=OFF
    -DENABLE_TIL=OFF
    -DENABLE_TSX=OFF
    -DENABLE_USGSDEM=OFF
    -DENABLE_WCS=OFF
    -DENABLE_WMS=OFF
    -DENABLE_WMTS=OFF
    -DENABLE_XPM=OFF
    -DENABLE_XYZ=OFF
    -DENABLE_ZMAP=OFF
    -DENABLE_AERONAVFAA=OFF
    -DENABLE_ARCGEN=OFF
    -DENABLE_AVC=OFF
    -DENABLE_BNA=OFF
    -DENABLE_CARTODB=OFF
    -DENABLE_CLOUDANT=OFF
    -DENABLE_COUCHDB=OFF
    -DENABLE_CSV=OFF
    -DENABLE_CSW=OFF
    -DENABLE_DGN=OFF
    -DENABLE_DXF=OFF
    -DENABLE_EDIGEO=OFF
    -DENABLE_ELASTIC=OFF
    -DENABLE_GEOCONCEPT=OFF
    -DENABLE_GEORSS=OFF
    -DENABLE_GFT=OFF
    -DENABLE_GML=OFF
    -DENABLE_GMT=OFF
    -DENABLE_GPSBABEL=OFF
#    -DENABLE_GPX=OFF
    -DENABLE_GTM=OFF
    -DENABLE_HTF=OFF
    -DENABLE_IDRISI_VECTOR=OFF
    -DENABLE_JML=OFF
    -DENABLE_NTF=OFF
    -DENABLE_ODS=OFF
    -DENABLE_OPENAIR=OFF
    -DENABLE_OPENFILEGDB=OFF
    -DENABLE_OSM=OFF
    -DENABLE_PDS_VECTOR=OFF
    -DENABLE_PG=OFF
    -DENABLE_PGDUMP=OFF
    -DENABLE_REC=OFF
    -DENABLE_S57=OFF
    -DENABLE_SDTS_VECTOR=OFF
    -DENABLE_SEGUKOOA=OFF
    -DENABLE_SEGY=OFF
    -DENABLE_SELAFIN=OFF
#    -DENABLE_SHAPE=OFF
#    -DENABLE_SQLITE_GPKG=OFF
    -DENABLE_SUA=OFF
    -DENABLE_SVG=OFF
    -DENABLE_SXF=OFF
    -DENABLE_TIGER=OFF
    -DENABLE_VDV=OFF
    -DENABLE_VFK=OFF
    -DENABLE_WASP=OFF
    -DENABLE_WFS=OFF
    -DENABLE_XLSX=OFF
    -DGDAL_BUILD_APPS=OFF
    -DGDAL_BUILD_DOCS=OFF)

if(BUILD_TARGET_PLATFORM STREQUAL "Desktop")
    # this only need for API report version
    if(WITH_GDAL_EXTERNAL)
        add_definitions (-DHAVE_CURLVER_H)
        add_definitions (-DHAVE_GEOS_C_H)
        add_definitions (-DHAVE_SQLITE3_H)
        add_definitions (-DHAVE_JSON_C_VERSION_H)
        add_definitions (-DHAVE_PROJ_API_H)
        add_definitions (-DHAVE_JPEGLIB_H)
        add_definitions (-DHAVE_TIFF_H)
        add_definitions (-DHAVE_GEOTIFF_H)
        add_definitions (-DHAVE_PNG_H)
        add_definitions (-DHAVE_EXPAT_H)
        add_definitions (-DHAVE_ICONV_H)
        add_definitions (-DHAVE_ZLIB_H)
        add_definitions (-DHAVE_OPENSSLV_H)
    else()
        #include <curl/curlver.h>
        find_path(CURL_PATH curlver.h PATHS /usr/include /usr/include/curl /usr/local/include /usr/local/include/curl)
        if(CURL_PATH)
            # cut curl
            string(REPLACE "curl" "" CURL_PATH ${CURL_PATH})
            include_directories (${CURL_PATH})
            add_definitions (-DHAVE_CURLVER_H)
        endif()
        #include "geos_c.h"
        find_path(GEOS_PATH geos_c.h PATHS /usr/include /usr/include/geos /usr/local/include /usr/local/include/geos)
        if(GEOS_PATH)
            include_directories (${GEOS_PATH})
            add_definitions (-DHAVE_GEOS_C_H)
        endif()
        #include "sqlite3.h"
        find_path(SQLITE3_PATH sqlite3.h PATHS /usr/include /usr/include/sqlite3 /usr/local/include /usr/local/include/sqlite3)
        if(SQLITE3_PATH)
            include_directories (${SQLITE3_PATH})
            add_definitions (-DHAVE_SQLITE3_H)
        endif()
        #include "json-c/json_c_version.h"
        find_path(JSON_C_PATH json_c_version.h PATHS /usr/include /usr/include/json-c /usr/local/include /usr/local/include/json-c)
        if(JSON_C_PATH)
            # cut json-c
            string(REPLACE "json-c" "" JSON_C_PATH ${JSON_C_PATH} )
            include_directories (${JSON_C_PATH})
            add_definitions (-DHAVE_JSON_C_VERSION_H)
        endif()
        #include "proj_api.h"
        find_path(PROJ_API_PATH proj_api.h PATHS /usr/include /usr/include/proj4 /usr/local/include /usr/local/include/proj4)
        if(PROJ_API_PATH)
            include_directories (${PROJ_API_PATH})
            add_definitions (-DHAVE_PROJ_API_H)
        endif()
        #include "jpeglib.h"
        find_path(JPEGLIB_PATH jpeglib.h PATHS /usr/include /usr/include/jpeg /usr/local/include /usr/local/include/jpeg)
        if(JPEGLIB_PATH)
            include_directories (${JPEGLIB_PATH})
            add_definitions (-DHAVE_JPEGLIB_H)
        endif()
        #include "tiff.h"
        find_path(TIFF_PATH tiff.h PATHS /usr/include /usr/include/tiff /usr/local/include /usr/local/include/tiff)
        if(TIFF_PATH)
            include_directories (${TIFF_PATH})
            add_definitions (-DHAVE_TIFF_H)
        endif()
        #include "geotiff.h"
        find_path(GEOTIFF_PATH geotiff.h PATHS /usr/include /usr/include/geotiff /usr/local/include /usr/local/include/geotiff)
        if(GEOTIFF_PATH)
            # cut geotiff
            # string(REPLACE "geotiff" "" GEOTIFF_PATH ${GEOTIFF_PATH})
            include_directories (${GEOTIFF_PATH})
            add_definitions (-DHAVE_GEOTIFF_H)
        endif()
        #include "png.h"
        find_path(PNG_PATH png.h PATHS /usr/include /usr/include/png /usr/local/include /usr/local/include/png)
        if(PNG_PATH)
            include_directories (${PNG_PATH})
            add_definitions (-DHAVE_PNG_H)
        endif()
        #include "expat.h"
        find_path(EXPAT_PATH expat.h PATHS /usr/include /usr/include/expat /usr/local/include /usr/local/include/expat)
        if(EXPAT_PATH)
            include_directories (${EXPAT_PATH})
            add_definitions (-DHAVE_EXPAT_H)
        endif()
        #include "iconv.h"
        find_path(ICONV_PATH iconv.h PATHS /usr/include /usr/include/iconv/ /usr/local/include /usr/local/include/iconv)
        if(ICONV_PATH)
            include_directories (${ICONV_PATH})
            add_definitions (-DHAVE_ICONV_H)
        endif()
        #include "zlib.h"
        find_path(ZLIB_PATH zlib.h PATHS /usr/include /usr/include/zlib /usr/local/include /usr/local/include/zlib)
        if(ZLIB_PATH)
            include_directories (${ZLIB_PATH})
            add_definitions (-DHAVE_ZLIB_H)
        endif()
        #include "openssl/opensslv.h"
        find_path(OPENSSL_PATH opensslv.h PATHS /usr/include /usr/include/openssl /usr/local/include /usr/local/include/openssl)
        if(OPENSSL_PATH)
            # cut openssl
            string(REPLACE "openssl" "" OPENSSL_PATH ${OPENSSL_PATH})
            include_directories (${OPENSSL_PATH})
            add_definitions (-DHAVE_OPENSSLV_H)
        endif()
    endif()
endif()
