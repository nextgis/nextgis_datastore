################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Author: NikitaFeodonit, nfeodonit@yandex.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public License v3
#
#  Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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
    layer.h
    map.h
    mapstore.h
    maptransform.h
    mapview.h
#    matrix.h
    overlay.h
#    vector.h
    gl/view.h
    gl/program.h
    gl/functions.h
    gl/style.h
    gl/buffer.h
    gl/layer.h
    gl/image.h
    gl/tile.h
    gl/overlay.h
)

set(CSOURCES
    layer.cpp
    map.cpp
    mapstore.cpp
    maptransform.cpp
    mapview.cpp
#    matrix.cpp
    overlay.cpp
    gl/view.cpp
    gl/program.cpp
    gl/functions.cpp
    gl/style.cpp
    gl/buffer.cpp
    gl/layer.cpp
    gl/image.cpp
    gl/tile.cpp
    gl/overlay.cpp
)

add_library(${LIB_NAME} OBJECT ${CSOURCES} ${HHEADERS})

set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 11
    C_STANDARD 11
)

set(OBJ_LIBRARIES ${OBJ_LIBRARIES} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
