################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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

set(LIB_NAME catalog)

set(HHEADERS
    object.h
    objectcontainer.h
    catalog.h
    file.h
    folder.h
    archive.h
    ${INCLUDE_DIR}/ngstore/catalog/filter.h
    localconnections.h
    mapfile.h
    factories/objectfactory.h
    factories/folderfactory.h
    factories/filefactory.h
    factories/datastorefactory.h
    factories/simpledatasetfactory.h
    factories/rasterfactory.h
)

set(CSOURCES
    object.cpp
    objectcontainer.cpp
    catalog.cpp
    file.cpp
    folder.cpp
    archive.cpp
    filter.cpp
    localconnections.cpp
    mapfile.cpp
    factories/objectfactory.cpp
    factories/folderfactory.cpp
    factories/filefactory.cpp
    factories/datastorefactory.cpp
    factories/simpledatasetfactory.cpp
    factories/rasterfactory.cpp
)

add_library(${LIB_NAME} OBJECT ${CSOURCES} ${HHEADERS})

set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 11
    C_STANDARD 11
)

set(OBJ_LIBRARIES ${OBJ_LIBRARIES} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
