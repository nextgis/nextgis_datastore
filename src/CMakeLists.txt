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

if(ANDROID OR (UNIX AND NOT APPLE))
    find_package(EGL REQUIRED)
    if(EGL_FOUND)
        include_directories(${EGL_INCLUDE_DIRS})
        set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${EGL_LIBRARIES})
    endif()

    find_package(GLES2 REQUIRED)
    if(OPENGLES2_FOUND)
        include_directories(${OPENGLES2_INCLUDE_DIRS})
        set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${OPENGLES2_LIBRARIES})
    endif()
elseif(IOS)
    find_package(GLES2 REQUIRED)
    if(OPENGLES2_FOUND)
        include_directories(${OPENGLES2_INCLUDE_DIRS})
        set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${OPENGLES2_LIBRARIES})
    endif()
else() # TODO: Windows?
    find_package(OpenGL REQUIRED)
    if(OPENGL_FOUND)
        include_directories(${OPENGL_INCLUDE_DIR})
        set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${OPENGL_LIBRARIES})
    endif()
endif()

set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${INCLUDE_DIR}
)

set(HHEADERS
    ${INCLUDE_DIR}/ngstore/api.h
    ${INCLUDE_DIR}/ngstore/version.h
    ${INCLUDE_DIR}/ngstore/common.h
    ${INCLUDE_DIR}/ngstore/codes.h
    ${INCLUDE_DIR}/ngstore/util/constants.h
    api_priv.h
)

set(CSOURCES
    api.cpp
)

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    if(NOT APPLE)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffunction-sections -Wl,--gc-sections -fno-asynchronous-unwind-tables -Wl,--strip-all")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffunction-sections -Wl,--gc-sections -fno-asynchronous-unwind-tables -Wl,--strip-all")
    endif()
endif()

set(OBJ_LIBRARIES)
add_subdirectory (ds)
add_subdirectory (catalog)
add_subdirectory (map)
add_subdirectory (util)

if(ANDROID)
    add_definitions(-DNGS_ABI="Android ${ANDROID_ABI}" -DNGS_MOBILE)
elseif(IOS)
    add_definitions(-DNGS_ABI="iOS" -DNGS_MOBILE)
else()
    if(WIN32)
        add_definitions(-DNGS_ABI="Windows")
    elseif(APPLE)
        add_definitions(-DNGS_ABI="Mac OS X")
    else()
        add_definitions(-DNGS_ABI="UNIX")
    endif()
endif()

set(LIB_NAME ${PROJECT_NAME})
if(WIN32)
    set (LIB_VERSION ${NGS_MAJOR_VERSION})
else ()
    set (LIB_VERSION ${VERSION})
endif()

if(BUILD_SHARED_LIBS)
    add_library(${LIB_NAME} SHARED ${CSOURCES} ${HHEADERS} ${OBJ_LIBRARIES})

    if(MSVC)
        set_target_properties(${LIB_NAME} PROPERTIES
            IMPORT_SUFFIX "_i.lib"
            SUFFIX "${NGS_MAJOR_VERSION}${NGS_MINOR_VERSION}.dll")
    endif()

    set_target_properties (${LIB_NAME} PROPERTIES
        VERSION ${LIB_VERSION}
        SOVERSION ${SOVERSION}
        DEFINE_SYMBOL NGSTOR_EXPORTS
    )

    if(BUILD_SHARED_LIBS AND BUILD_STATIC_LIBS)
        add_library(${LIB_NAME}static STATIC ${CSOURCES} ${HHEADERS} ${OBJ_LIBRARIES})
        if(WIN32)
            set_target_properties(${LIB_NAME}static PROPERTIES OUTPUT_NAME ${LIB_NAME}_i)
        else()
            set_target_properties(${LIB_NAME}static PROPERTIES OUTPUT_NAME ${LIB_NAME})
        endif()
        set_target_properties(${LIB_NAME}static PROPERTIES
            CXX_STANDARD 11
            C_STANDARD 11
            CLEAN_DIRECT_OUTPUT 1)
    endif()
else()
    add_library(${LIB_NAME} STATIC ${CSOURCES} ${HHEADERS} ${OBJ_LIBRARIES})
    set_target_properties (${LIB_NAME} PROPERTIES DEFINE_SYMBOL NGSTOR_STATIC)
endif()

set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 11
    C_STANDARD 11
)

target_link_extlibraries(${LIB_NAME})

set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${LIB_NAME} PARENT_SCOPE)
set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${LIB_NAME} PARENT_SCOPE)
set(LINK_SEARCH_PATHS ${LINK_SEARCH_PATHS} PARENT_SCOPE)
set(WITHOPT ${WITHOPT} PARENT_SCOPE)
set(EXPORTS_PATHS ${EXPORTS_PATHS} PARENT_SCOPE)

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL )
    install(TARGETS ${LIB_NAME}
        EXPORT ${LIB_NAME}
        RUNTIME DESTINATION ${INSTALL_BIN_DIR} COMPONENT libraries
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR} COMPONENT libraries
        LIBRARY DESTINATION ${INSTALL_LIB_DIR} COMPONENT libraries
    )
endif()
