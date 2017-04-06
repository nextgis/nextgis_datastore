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

set(LIB_NAME map)

set(HHEADERS
#    glfillers.h
#    glview.h
    layer.h
    map.h
    mapstore.h
    maptransform.h
    mapview.h
    matrix.h
    renderlayer.h
#    style.h
#    vector.h
)

set(CSOURCES
#    glfillers.cpp
#    glview.cpp
    layer.cpp
    map.cpp
    mapstore.cpp
    maptransform.cpp
    mapview.cpp
    matrix.cpp
    renderlayer.cpp
#    style.cpp
)

add_library(${LIB_NAME} OBJECT ${CSOURCES} ${HHEADERS})

#if(WITH_CGAL_EXTERNAL)
#    add_dependencies(${LIB_NAME} CGAL_EP)
#endif()

#if(WITH_JSONC_EXTERNAL)
#    add_dependencies(${LIB_NAME} JSONC_EP)
#endif()

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
