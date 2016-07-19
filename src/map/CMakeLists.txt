################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public Licens v3
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

set(LIB_NAME map)

set(HHEADERS
)

set(HHEADER_PRIV
    mapstore.h
    map.h
    maptransform.h
    mapview.h
    matrix.h
    glview.h
)

set(CSOURCES
    mapstore.cpp
    map.cpp
    maptransform.cpp
    mapview.cpp
    matrix.cpp
    glview.cpp
)

add_library(${LIB_NAME} OBJECT ${CSOURCES} ${HHEADERS} ${HHEADER_PRIV})

set_property(TARGET ${LIB_NAME} PROPERTY CXX_STANDARD 11)
set_property(TARGET ${LIB_NAME} PROPERTY C_STANDARD 11)

set(OBJ_LIBRARIES ${OBJ_LIBRARIES} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)

if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL)
    install(FILES ${HHEADERS} DESTINATION "${INSTALL_INC_DIR}" 
        COMPONENT headers)
endif()
