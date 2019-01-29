################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public Licens v3
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

set(LIB_NAME util)

set(HHEADERS
    buffer.h
    stringutil.h
    versionutil.h
    settings.h
    error.h
    progress.h
    options.h
    notify.h
    threadpool.h
    authstore.h
    url.h
    mutex.h
    account.h
)

set(CSOURCES
    buffer.cpp
    stringutil.cpp
    versionutil.cpp
    settings.cpp
    error.cpp
    progress.cpp
    options.cpp
    notify.cpp
    threadpool.cpp
    authstore.cpp
    url.cpp
    mutex.cpp
    account.cpp
)

set_property(SOURCE url.cpp APPEND_STRING PROPERTY CMAKE_CXX_FLAGS " -Wdisabled-macro-expansion ")

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-multichar")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-multichar")
endif()

add_library(${LIB_NAME} OBJECT ${CSOURCES} ${HHEADERS})
    
set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 11
    C_STANDARD 11
)

set(OBJ_LIBRARIES ${OBJ_LIBRARIES} $<TARGET_OBJECTS:${LIB_NAME}> PARENT_SCOPE)
