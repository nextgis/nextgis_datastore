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

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

# some init settings
set(CMAKE_COLOR_MAKEFILE ON)
# set path to additional CMake modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(PACKAGE_ID "com.nextgis.store")

set(BUILD_TARGET_PLATFORM "Desktop" CACHE STRING "Select build target platform")
set_property(CACHE BUILD_TARGET_PLATFORM PROPERTY STRINGS "ANDROID" "IOS" "Desktop")

if(BUILD_TARGET_PLATFORM STREQUAL "ANDROID")
    include(android)
elseif(BUILD_TARGET_PLATFORM STREQUAL "IOS")
    include(ios)    
endif()


project(ngstore)

include(GNUInstallDirs)
set(INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR} CACHE INTERNAL "Installation directory for executables" FORCE)
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE INTERNAL "Installation directory for libraries" FORCE)
set(INSTALL_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR}/ngstore CACHE INTERNAL "Installation directory for headers" FORCE)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-D_DEBUG)
endif()

include(util)

configure_file(${CMAKE_MODULE_PATH}/uninstall.cmake.in ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake IMMEDIATE @ONLY)

check_version(NGM_MAJOR_VERSION NGM_MINOR_VERSION NGM_REV_VERSION)
set(VERSION ${NGM_MAJOR_VERSION}.${NGM_MINOR_VERSION}.${NGM_REV_VERSION})
set(SOVERSION 1)

report_version(${PROJECT_NAME} ${VERSION})

enable_testing()
include(FindAnyProject)

add_subdirectory(src)
add_subdirectory(bindings)
add_subdirectory(test)

# uninstall
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
