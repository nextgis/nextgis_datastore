################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public Licens v3
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

if(ANDROID OR (UNIX AND NOT APPLE))
    find_package(EGL REQUIRED)
    if(EGL_FOUND)
        include_directories(${EGL_INCLUDE_DIRS})
        set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${EGL_LIBRARIES})
        set(OPENGL_FOUND ON)
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
        set(OPENGL_FOUND ON)
    endif()
elseif(MSVC)
	# TODO: Windows?
	set(OPENGL_FOUND OFF)
else()
    find_package(OpenGL REQUIRED)
    if(OPENGL_FOUND)
        include_directories(${OPENGL_INCLUDE_DIR})
        set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${OPENGL_LIBRARIES})
    endif()
endif()

set(INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${INCLUDE_DIR}
)

set(HHEADERS_PUB
    ${INCLUDE_DIR}/ngstore/api.h
    ${INCLUDE_DIR}/ngstore/version.h
    ${INCLUDE_DIR}/ngstore/common.h
    ${INCLUDE_DIR}/ngstore/codes.h
    ${INCLUDE_DIR}/ngstore/util/constants.h
)

set(HHEADERS ${HHEADERS_PUB}
    api_priv.h
)

set(CSOURCES
    api.cpp
)

if(ANDROID)
    set(CSOURCES ${CSOURCES}
        api_jni.cpp
    )

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffunction-sections -Wl,--gc-sections -fno-asynchronous-unwind-tables -Wl,--strip-all")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -ffunction-sections -Wl,--gc-sections -fno-asynchronous-unwind-tables -Wl,--strip-all")
endif()

if(IOS)
  set(CMAKE_XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN "YES")
  set(CMAKE_XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN "YES")

  set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE ${ENABLE_BITCODE})
  option(NGS_DISABLE_DSYM "Disable generation of dSYM information" OFF)
  if(NGS_DISABLE_DSYM)
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf")
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Release] "dwarf")
  else()
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT "dwarf-with-dsym")
    set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=Release] "dwarf-with-dsym")

    # Generate debug symbols but strip them (i.e., clang or gcc)
    set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS "YES")
    set(CMAKE_XCODE_ATTRIBUTE_GCC_GENERATE_DEBUGGING_SYMBOLS[variant=Release] "YES")

    # Add -g to force dSYM generation
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
  endif()

  # RelWithDebInfo should always have symbols
  set(CMAKE_XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT[variant=RelWithDebInfo] "dwarf-with-dsym")
  set(CMAKE_XCODE_ATTRIBUTE_GENERATE_PROFILING_CODE[variant=RelWithDebInfo] "YES")

  # Minimize static analyzer time
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_STATIC_ANALYZER_MODE_ON_ANALYZE_ACTION "Shallow")
  set(CMAKE_XCODE_ATTRIBUTE_CLANG_STATIC_ANALYZER_MODE "Shallow")

endif()

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

set(OBJ_LIBRARIES)
add_subdirectory (ds)
add_subdirectory (catalog)
add_subdirectory (map)
add_subdirectory (util)


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
            SUFFIX "${NGS_MAJOR_VERSION}.dll")
    endif()

    if(NOT IOS)
        set_target_properties (${LIB_NAME} PROPERTIES
            VERSION ${LIB_VERSION}
            SOVERSION ${SOVERSION}
        )
    endif()
    set_target_properties (${LIB_NAME} PROPERTIES
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
elseif(OSX_FRAMEWORK)
    add_library(${LIB_NAME} SHARED ${CSOURCES} ${HHEADERS} ${OBJ_LIBRARIES})
    set_property(SOURCE ${HHEADERS_PUB}
        PROPERTY MACOSX_PACKAGE_LOCATION "Headers/ngstore"
    )
    set_target_properties(${LIB_NAME} PROPERTIES
        FRAMEWORK TRUE
        FRAMEWORK_VERSION "${NGS_MAJOR_VERSION}"
        VERSION ${VERSION}
        MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${VERSION}
        MACOSX_FRAMEWORK_BUNDLE_VERSION ${VERSION}
        MACOSX_FRAMEWORK_IDENTIFIER com.nextgis.datastore
        XCODE_ATTRIBUTE_INSTALL_PATH "@rpath"
        # PUBLIC_HEADER "${HHEADERS_PUB}"
    )    
else()
    add_library(${LIB_NAME} STATIC ${CSOURCES} ${HHEADERS} ${OBJ_LIBRARIES})
    set_target_properties(${LIB_NAME} PROPERTIES DEFINE_SYMBOL NGSTOR_STATIC)
endif()

if(IOS)
    file(STRINGS ${PROJECT_SOURCE_DIR}/include/ngstore/api.h apis REGEX "ngs[a-zA-z0-9]*\\(" )
    set(apis2)
    foreach(api ${apis})
        string(SUBSTRING ${api} 0 2 testComment)
        if(NOT testComment STREQUAL "//")
            string(REGEX MATCH "ngs[a-zA-z0-9]*\\(" api2 ${api})
            string(REPLACE "(" "" api3 ${api2})
            string(STRIP ${api3} api4 )
            set(apis2 "${apis2}\n_${api4}")
        endif()
    endforeach()
    set(apis2 "${apis2}\n")
    message(STATUS "Exported functions: ${apis2}")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/sym.exp ${apis2})
    set_target_properties(${LIB_NAME} PROPERTIES
        COMPILE_FLAGS "-arch ${IOS_ARCH}"
        LINK_FLAGS "-arch ${IOS_ARCH} -Wl,-exported_symbols_list,${CMAKE_CURRENT_BINARY_DIR}/sym.exp"
    )
endif()

set_target_properties(${LIB_NAME} PROPERTIES
    CXX_STANDARD 11
    C_STANDARD 11
    CXX_VISIBILITY_PRESET hidden
    C_VISIBILITY_PRESET hidden
)

target_link_extlibraries(${LIB_NAME})

set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${LIB_NAME} PARENT_SCOPE)
set(DEPENDENCY_LIB ${DEPENDENCY_LIB} ${LIB_NAME} PARENT_SCOPE)
set(LINK_SEARCH_PATHS ${LINK_SEARCH_PATHS} PARENT_SCOPE)
set(WITHOPT ${WITHOPT} PARENT_SCOPE)
set(EXPORTS_PATHS ${EXPORTS_PATHS} PARENT_SCOPE)

set(PACKAGE_NAME ${LIB_NAME})

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    install(TARGETS ${LIB_NAME}
        EXPORT ${PACKAGE_UPPER_NAME}Targets
        RUNTIME DESTINATION ${INSTALL_BIN_DIR}
        LIBRARY DESTINATION ${INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${INSTALL_LIB_DIR}
        INCLUDES DESTINATION ${INSTALL_INC_DIR}
        FRAMEWORK DESTINATION ${INSTALL_LIB_DIR}
    )    
endif()

if(NOT SKIP_INSTALL_HEADERS AND NOT SKIP_INSTALL_ALL )
    install(FILES ${HHEADERS_PUB} DESTINATION "${INSTALL_INC_DIR}"  COMPONENT headers)
endif()

# Add path to includes to build-tree export
target_include_directories(${LIB_NAME} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${PROJECT_CURRENT_BINARY_DIR}/include>
)

# Add all targets to the build-tree export set
export(TARGETS ${LIB_NAME}
    FILE ${PROJECT_BINARY_DIR}/${PACKAGE_UPPER_NAME}Targets.cmake)
    
# Create the <Package>Config.cmake file
configure_file(${PROJECT_SOURCE_DIR}/cmake/PackageConfig.cmake.in
    ${PROJECT_BINARY_DIR}/${PACKAGE_UPPER_NAME}Config.cmake @ONLY)

if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    # Install the <Package>Config.cmake
    install(FILES
      ${PROJECT_BINARY_DIR}/${PACKAGE_UPPER_NAME}Config.cmake
      DESTINATION ${INSTALL_CMAKECONF_DIR} COMPONENT dev)

    # Install the export set for use with the install-tree
    install(EXPORT ${PACKAGE_UPPER_NAME}Targets DESTINATION ${INSTALL_CMAKECONF_DIR} COMPONENT dev)
endif()