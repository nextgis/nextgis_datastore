################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public License v3
#
#  Copyright (c) 2016,2017 NextGIS, <info@nextgis.com>
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

set(LIB_NAME ds)

set(HHEADERS
    datastore.h
    memstore.h
    dataset.h
    simpledataset.h
    featureclass.h
    raster.h
    table.h
    storefeatureclass.h
    coordinatetransformation.h
    geometry.h
)

set(CSOURCES
    datastore.cpp
    memstore.cpp
    dataset.cpp
    simpledataset.cpp
    featureclass.cpp
    raster.cpp
    table.cpp
    storefeatureclass.cpp
    coordinatetransformation.cpp
    geometry.cpp
)

# Triangulation by MapBox
set(HHEADERS ${HHEADERS}
    earcut.hpp
)

set(CSOURCES ${CSOURCES}

)

add_library(${LIB_NAME} OBJECT ${CSOURCES} ${HHEADERS})

#if(WITH_OpenSSL_EXTERNAL)
#    add_dependencies(${LIB_NAME} OpenSSL_EP)
#endif()

#if(WITH_CURL_EXTERNAL)
#    add_dependencies(${LIB_NAME} CURL_EP)
#endif()

if(WITH_GDAL_EXTERNAL)
    add_dependencies(${LIB_NAME} GDAL_EP)
endif()

set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 11
    C_STANDARD 11
)

set(OBJ_LIBRARIES ${OBJ_LIBRARIES} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
