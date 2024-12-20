################################################################################
#  Project: libngstore
#  Purpose: NextGIS store support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public License v3
#
#  Copyright (c) 2016-2024 NextGIS, <info@nextgis.com>
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

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

# some init settings
set(CMAKE_COLOR_MAKEFILE ON)
# set path to additional CMake modules
set(CMAKE_MODULE_PATH 
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake 
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/borsch/cmake 
    ${CMAKE_MODULE_PATH}
)

project(ngstore)

include(base)
include(util)

check_version(NGS_MAJOR_VERSION NGS_MINOR_VERSION NGS_REV_VERSION)
set(VERSION ${NGS_MAJOR_VERSION}.${NGS_MINOR_VERSION}.${NGS_REV_VERSION})
set(SOVERSION 1)

report_version(${PROJECT_NAME} ${VERSION})

include(FindAnyProject)
include(extlib)

add_subdirectory(src)
add_subdirectory(include)

enable_testing()
add_subdirectory(test)

# uninstall
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

include(pack)
pack(PACKAGE_NAME PACKAGE_VENDOR VERSION)