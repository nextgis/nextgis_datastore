################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public License v3
#
#  Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
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
set(REQUIREMENTS JBIG ICONV SQLite3 CURL PROJ EXPAT TIFF GeoTIFF JPEG PNG LibXml2 LibLZMA)

if(NOT BUILD_TARGET_PLATFORM STREQUAL "Desktop")

    foreach(REQUIREMENT ${REQUIREMENTS})
        set(WITH_${REQUIREMENT} ON CACHE BOOL "${REQUIREMENT} on" FORCE)
        set(WITH_${REQUIREMENT}_EXTERNAL ON CACHE BOOL "${REQUIREMENT} external on" FORCE)
    endforeach()
    

    set(WITH_OpenSSL ON CACHE BOOL "OpenSSL on" FORCE)
    set(WITH_GEOS ON CACHE BOOL "GEOS on" FORCE)
    set(WITH_GDAL ON CACHE BOOL "GDAL on" FORCE)
    set(WITH_ZLIB ON CACHE BOOL "ZLIB on" FORCE)
    set(WITH_JSONC ON CACHE BOOL "JSONC on" FORCE)
    # set(WITH_ICONV ON CACHE BOOL "ICONV on" FORCE)

    # Not needed set(WITH_Boost ON CACHE BOOL "Boost on" FORCE)
    # Not needed set(WITH_CGAL ON CACHE BOOL "CGAL on" FORCE)

    set(WITH_OpenSSL_EXTERNAL ON CACHE BOOL "OpenSSL external on" FORCE)
    set(WITH_GEOS_EXTERNAL ON CACHE BOOL "GEOS external on" FORCE)
    set(WITH_GDAL_EXTERNAL ON CACHE BOOL "GDAL external on" FORCE)
    set(WITH_JSONC_EXTERNAL ON CACHE BOOL "JSONC external on" FORCE)

    # Not needed set(WITH_Boost_EXTERNAL ON CACHE BOOL "Boost external on" FORCE)
    # Not needed set(WITH_CGAL_EXTERNAL ON CACHE BOOL "CGAL external on" FORCE)

    # if(ANDROID)
    #     set(WITH_ICONV_EXTERNAL ON CACHE BOOL "iconv external on")
    # else()
    #     set(WITH_ICONV_EXTERNAL OFF CACHE BOOL "iconv external on")
    # endif()

    set(WITH_JPEG12 OFF CACHE BOOL "JPEG12 off" FORCE)
    set(WITH_JPEG12_EXTERNAL OFF CACHE BOOL "JPEG12 external off" FORCE)
    # set(WITH_LibLZMA OFF CACHE BOOL "LibLZMA off" FORCE)
    # set(WITH_LibLZMA_EXTERNAL OFF CACHE BOOL "LibLZMA external off" FORCE)
    set(WITH_PostgreSQL OFF CACHE BOOL "PostgreSQL off" FORCE)
    set(WITH_PostgreSQL_EXTERNAL OFF CACHE BOOL "PostgreSQL external off" FORCE)
    # if(ANDROID)
    #     set(WITH_LibXml2 OFF CACHE BOOL "LibXml2 off" FORCE)
    #     set(WITH_LibXml2_EXTERNAL OFF CACHE BOOL "LibXml2 external off" FORCE)
    # else()
    #     set(WITH_LibXml2 ON CACHE BOOL "LibXml2 off" FORCE)
    # endif()

    foreach(REQUIREMENT ${REQUIREMENTS})
        find_anyproject(${REQUIREMENT} REQUIRED SHARED OFF)
    endforeach()
    find_anyproject(JSONC REQUIRED NAMES jsonc json-c SHARED OFF)

    set(THIRD_PARTY_INCLUDE_PATH ${CMAKE_BINARY_DIR}/third-party)

    macro( find_exthost_path )
        set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )
        set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER )
        set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER )

        find_path( ${ARGN} )

        set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
        set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
        set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )
    endmacro()

    macro(FIND_HEADERS lib header have)
        string(TOUPPER ${lib} UPPER_LIB)

        find_exthost_path(${lib}_PATH "${header}"
            PATHS
                ${${UPPER_LIB}_INCLUDE_DIRS}
                "${THIRD_PARTY_INCLUDE_PATH}/src/${lib}_EP"
                "${THIRD_PARTY_INCLUDE_PATH}/build/${lib}_EP-build"
            PATH_SUFFIXES
                "src" "include" "libtiff" "lib" "libcurl" "curl"
        )
        if(${lib}_PATH)
            message(STATUS "Include path: ${${lib}_PATH}")
            include_directories(${${lib}_PATH})
            add_definitions(-D${have})
        endif()
    endmacro()

    find_headers(SQLite3 sqlite3.h HAVE_SQLITE3_H)
    find_headers(JSONC json_c_version.h HAVE_JSON_C_VERSION_H)
    find_headers(PROJ proj.h HAVE_PROJ_H)
    find_headers(JPEG jpeglib.h HAVE_JPEGLIB_H)
    find_headers(TIFF tiff.h HAVE_TIFF_H)
    find_headers(GeoTIFF geotiff.h HAVE_GEOTIFF_H)
    find_headers(PNG png.h HAVE_PNG_H)
    find_headers(EXPAT expat.h HAVE_EXPAT_H)
    find_headers(ICONV iconv.h HAVE_ICONV_H)
    find_headers(CURL curlver.h HAVE_CURLVER_H)

    add_definitions (-DHAVE_GEOS_C_H)
    # add_definitions (-DHAVE_JSON_C_VERSION_H)
    # add_definitions (-DHAVE_JPEGLIB_H)
    # add_definitions (-DHAVE_TIFF_H)
    # add_definitions (-DHAVE_GEOTIFF_H)
    # add_definitions (-DHAVE_PNG_H)
    # add_definitions (-DHAVE_EXPAT_H)
    # add_definitions (-DHAVE_ICONV_H)
    add_definitions (-DHAVE_ZLIB_H)
    add_definitions (-DHAVE_OPENSSLV_H)

endif()

# Not needed find_anyproject(CGAL REQUIRED SHARED OFF)
# Not needed find_anyproject(Boost REQUIRED)

find_anyproject(OpenSSL REQUIRED SHARED OFF CMAKE_ARGS
    -DOPENSSL_NO_AFALGENG=ON
    -DOPENSSL_NO_ASM=ON # Not working for mobile platform yet
    -DOPENSSL_NO_DYNAMIC_ENGINE=ON
    -DOPENSSL_NO_STATIC_ENGINE=OFF
    -DOPENSSL_NO_DEPRECATED=ON
    -DOPENSSL_NO_UNIT_TEST=ON
) # TODO: Disable max alghorithms. Check TLS v1.2 working!

# For fast cut by envelope
find_anyproject(GEOS REQUIRED SHARED OFF CMAKE_ARGS
    -DGEOS_ENABLE_TESTS=OFF
)

find_anyproject(GDAL REQUIRED SHARED OFF CMAKE_ARGS
    -DENABLE_MRF=OFF -DENABLE_PLSCENES=OFF -DENABLE_AAIGRID_GRASSASCIIGRID=OFF
    -DENABLE_ADRG_SRP=OFF -DENABLE_AIG=OFF -DENABLE_AIRSAR=OFF -DENABLE_ARG=OFF
    -DENABLE_BLX=OFF -DENABLE_BMP=OFF -DENABLE_BSB=OFF -DENABLE_CALS=OFF
    -DENABLE_CEOS=OFF -DENABLE_CEOS2=OFF -DENABLE_COASP=OFF -DENABLE_COSAR=OFF
    -DENABLE_CTG=OFF -DENABLE_DIMAP=OFF -DENABLE_DTED=OFF -DENABLE_E00GRID=OFF
    -DENABLE_EEDA=OFF -DENABLE_ELAS=OFF -DENABLE_ENVISAT=OFF -DENABLE_ERS=OFF
    -DENABLE_FIT=OFF -DENABLE_GFF=OFF -DENABLE_GIF=OFF -DENABLE_GRIB=OFF
    -DENABLE_GSAG_GSBG_GS7BG=OFF -DENABLE_GXF=OFF -DENABLE_HF2=OFF
    -DENABLE_IDRISI_RASTER=OFF -DENABLE_IGNFHeightASCIIGrid=OFF
    -DENABLE_ILWIS=OFF -DENABLE_INGR=OFF -DENABLE_IRIS=OFF -DENABLE_JAXAPALSAR=OFF
    -DENABLE_JDEM=OFF -DENABLE_KMLSUPEROVERLAY=OFF -DENABLE_L1B=OFF
    -DENABLE_LEVELLER=OFF -DENABLE_MAP=OFF -DENABLE_MBTILES=OFF
    -DENABLE_MSGN=OFF -DENABLE_NGSGEOID=OFF -DENABLE_NITF_RPFTOC_ECRGTOC=OFF
    -DENABLE_NWT=OFF -DENABLE_OZI=OFF -DENABLE_PCIDSK=OFF
    -DENABLE_PDS_ISIS2_ISIS3_VICAR=OFF -DENABLE_PLMOSAIC=OFF
#    -DENABLE_PNG=OFF
    -DENABLE_POSTGISRASTER=OFF -DENABLE_PRF=OFF -DENABLE_R=OFF
    -DENABLE_RASTERLITE=OFF -DENABLE_RIK=OFF -DENABLE_RMF=OFF -DENABLE_RDA=OFF
    -DENABLE_RS2=OFF -DENABLE_SAFE=OFF -DENABLE_SAGA=OFF -DENABLE_SENTINEL2=OFF
    -DENABLE_SIGDEM=OFF -DENABLE_SDTS_RASTER=OFF -DENABLE_SGI=OFF
    -DENABLE_SRTMHGT=OFF -DENABLE_TERRAGEN=OFF -DENABLE_TIL=OFF -DENABLE_TSX=OFF
    -DENABLE_USGSDEM=OFF -DENABLE_WCS=OFF
#    -DENABLE_WMS=OFF
    -DENABLE_WMTS=OFF -DENABLE_XPM=OFF -DENABLE_XYZ=OFF -DENABLE_ZMAP=OFF
    -DENABLE_AERONAVFAA=OFF -DENABLE_ARCGEN=OFF -DENABLE_AVC=OFF
    -DENABLE_BNA=OFF -DENABLE_CARTO=OFF -DENABLE_CLOUDANT=OFF
    -DENABLE_COUCHDB=OFF
    -DENABLE_CSV=OFF
    -DENABLE_CSW=OFF -DENABLE_DGN=OFF -DENABLE_DXF=OFF -DENABLE_EDIGEO=OFF
    -DENABLE_ELASTIC=OFF -DENABLE_GEOCONCEPT=OFF -DENABLE_GEORSS=OFF
    -DENABLE_GFT=OFF -DENABLE_GML=OFF -DENABLE_GMT=OFF -DENABLE_GPSBABEL=OFF
#    -DENABLE_GPX=OFF
    -DENABLE_GTM=OFF -DENABLE_HTF=OFF -DENABLE_IDRISI_VECTOR=OFF
    -DENABLE_JML=OFF -DENABLE_NTF=OFF -DENABLE_ODS=OFF -DENABLE_OPENAIR=OFF
    -DENABLE_OPENFILEGDB=OFF -DENABLE_OSM=OFF -DENABLE_PDS_VECTOR=OFF
    -DENABLE_PG=OFF -DENABLE_PGDUMP=OFF -DENABLE_REC=OFF -DENABLE_S57=OFF
    -DENABLE_SDTS_VECTOR=OFF -DENABLE_SEGUKOOA=OFF -DENABLE_SEGY=OFF
    -DENABLE_SELAFIN=OFF
#    -DENABLE_SHAPE=OFF
#    -DENABLE_SQLITE_GPKG=OFF
    -DENABLE_SUA=OFF -DENABLE_SVG=OFF -DENABLE_SXF=OFF -DENABLE_TIGER=OFF
    -DENABLE_VDV=OFF -DENABLE_VFK=OFF -DENABLE_WASP=OFF -DENABLE_WFS=OFF
    -DENABLE_XLSX=OFF -DENABLE_CAD=OFF
    -DGDAL_BUILD_APPS=OFF -DGDAL_BUILD_DOCS=OFF -DENABLE_NULL=OFF -DENABLE_NGW=ON 
    -DENABLE_GNMFILE=OFF -DENABLE_ECW=OFF -DENABLE_GEORASTER=OFF -DENABLE_HDF4=OFF 
    -DENABLE_MRSID=OFF -DENABLE_OPENJPEG=OFF -DENABLE_WEBP=OFF -DENABLE_LIBKML=OFF 
    -DENABLE_MVT=OFF -DENABLE_OCI=OFF -DENABLE_RAW=OFF
    -DGDAL_BUILD_APPS=OFF
    -DGDAL_BUILD_DOCS=OFF)

if(NOT BUILD_TARGET_PLATFORM STREQUAL "Desktop")   
    # Copy data to shared
    macro(copy_share INC_DIR DIR GET_PARENT)
        get_filename_component(PARENT_DIR ${INC_DIR} DIRECTORY)
        if(${GET_PARENT})
            get_filename_component(PARENT_DIR ${PARENT_DIR} DIRECTORY)
        endif()
        file(COPY ${PARENT_DIR}/share/${DIR}/ DESTINATION ${PROJECT_BINARY_DIR}/data)
    endmacro()

    copy_share(${PROJ_INCLUDE_DIRS} "proj" FALSE)
    copy_share(${GDAL_INCLUDE_DIRS} "gdal" TRUE)
    copy_share(${OPENSSL_INCLUDE_DIRS} "ssl/certs" FALSE)
endif()

if(BUILD_TARGET_PLATFORM STREQUAL "Desktop")
    # this only need for API report version

    set(THIRD_PARTY_INCLUDE_PATH ${CMAKE_BINARY_DIR}/third-party/install/include)

    #include <curl/curlver.h>
    find_path(CURL_PATH curlver.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/curl
        /usr/include
        /usr/include/curl
        /usr/local/include
        /usr/local/include/curl)
    if(CURL_PATH)
        # cut curl
        string(REPLACE "curl" "" CURL_PATH ${CURL_PATH})
        include_directories (${CURL_PATH})
        add_definitions (-DHAVE_CURLVER_H)
    endif()

    #include "geos_c.h"
    find_path(GEOS_PATH geos_c.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/geos
        /usr/include
        /usr/include/geos
        /usr/local/include
        /usr/local/include/geos)
    if(GEOS_PATH)
        include_directories (${GEOS_PATH})
        add_definitions (-DHAVE_GEOS_C_H)
    endif()

    #include "sqlite3.h"
    find_path(SQLITE3_PATH sqlite3.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/sqlite3
        /usr/include
        /usr/include/sqlite3
        /usr/local/include
        /usr/local/include/sqlite3)
    if(SQLITE3_PATH)
        include_directories (${SQLITE3_PATH})
        add_definitions (-DHAVE_SQLITE3_H)
    endif()

    #[[include "json-c/json_c_version.h"
    find_path(JSON_C_PATH json_c_version.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/json-c
        /usr/include
        /usr/include/json-c
        /usr/local/include
        /usr/local/include/json-c)
    if(JSON_C_PATH)
        include_directories (${JSON_C_PATH})
        add_definitions (-DHAVE_JSON_C_VERSION_H)
    endif()
    ]]

    #include "proj_api.h"
    find_path(PROJ_API_PATH proj_api.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/proj4
        /usr/include
        /usr/include/proj4
        /usr/local/include
        /usr/local/include/proj4)
    if(PROJ_API_PATH)
        include_directories (${PROJ_API_PATH})
        add_definitions (-DHAVE_PROJ_API_H)
    endif()

    #include "jpeglib.h"
    find_path(JPEGLIB_PATH jpeglib.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/jpeg
        /usr/include
        /usr/include/jpeg
        /usr/local/include
        /usr/local/include/jpeg)
    if(JPEGLIB_PATH)
        include_directories (${JPEGLIB_PATH})
        add_definitions (-DHAVE_JPEGLIB_H)
    endif()

    #include "tiff.h"
    find_path(TIFF_PATH tiff.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/tiff
        /usr/include
        /usr/include/tiff
        /usr/local/include
        /usr/local/include/tiff)
    if(TIFF_PATH)
        include_directories (${TIFF_PATH})
        add_definitions (-DHAVE_TIFF_H)
    endif()

    #include "geotiff.h"
    find_path(GEOTIFF_PATH geotiff.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/geotiff
        /usr/include /usr/include/geotiff
        /usr/local/include
        /usr/local/include/geotiff)
    if(GEOTIFF_PATH)
        # cut geotiff
        # string(REPLACE "geotiff" "" GEOTIFF_PATH ${GEOTIFF_PATH})
        include_directories (${GEOTIFF_PATH})
        add_definitions (-DHAVE_GEOTIFF_H)
    endif()

    #include "png.h"
    find_path(PNG_PATH png.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/libpng16
        /usr/include
        /usr/include/png
        /usr/local/include
        /usr/local/include/png)
    if(PNG_PATH)
        include_directories (${PNG_PATH})
        add_definitions (-DHAVE_PNG_H)
    endif()

    #include "expat.h"
    find_path(EXPAT_PATH expat.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/expat
        /usr/include
        /usr/include/expat
        /usr/local/include
        /usr/local/include/expat)
    if(EXPAT_PATH)
        include_directories (${EXPAT_PATH})
        add_definitions (-DHAVE_EXPAT_H)
    endif()

    #include "iconv.h"
    find_path(ICONV_PATH iconv.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/iconv
        /usr/include
        /usr/include/iconv/
        /usr/local/include
        /usr/local/include/iconv)
    if(ICONV_PATH)
        include_directories (${ICONV_PATH})
        add_definitions (-DHAVE_ICONV_H)
    endif()

    #include "zlib.h"
    find_path(ZLIB_PATH zlib.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/zlib
        /usr/include
        /usr/include/zlib
        /usr/local/include
        /usr/local/include/zlib)
    if(ZLIB_PATH)
        include_directories (${ZLIB_PATH})
        add_definitions (-DHAVE_ZLIB_H)
    endif()

    #include "openssl/opensslv.h"
    find_path(OPENSSL_PATH opensslv.h PATHS
        ${THIRD_PARTY_INCLUDE_PATH}
        ${THIRD_PARTY_INCLUDE_PATH}/openssl
        /usr/include
        /usr/include/openssl
        /usr/local/include
        /usr/local/include/openssl)
    if(OPENSSL_PATH)
        # cut openssl
        string(REPLACE "openssl" "" OPENSSL_PATH ${OPENSSL_PATH})
        include_directories (${OPENSSL_PATH})
        add_definitions (-DHAVE_OPENSSLV_H)
    endif()

# Not needed
#    #include "boost/version.hpp"
#    find_path(BOOST_PATH version.hpp PATHS
#        ${THIRD_PARTY_INCLUDE_PATH}/boost
#        /usr/include/boost
#        /usr/local/include/boost)
#    if(BOOST_PATH)
#        # cut boost
#        string(REPLACE "boost" "" BOOST_PATH ${BOOST_PATH})
#        include_directories (${BOOST_PATH})
#        add_definitions (-DHAVE_BOOST_VERSION_HPP)
#    endif()

#    #include "CGAL/version.h"
#    find_path(CGAL_PATH version.h PATHS
#        ${THIRD_PARTY_INCLUDE_PATH}/CGAL
#        /usr/include/CGAL
#        /usr/local/include/CGAL)
#    if(CGAL_PATH)
#        # cut CGAL
#        string(REPLACE "CGAL" "" CGAL_PATH ${CGAL_PATH})
#        include_directories (${CGAL_PATH})
#        add_definitions (-DHAVE_CGAL_VERSION_H)
#    endif()

endif()
