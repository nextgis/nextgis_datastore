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

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    include(CTest)

    set(WITH_GTest ON CACHE BOOL "Google test on")
    set(WITH_GTest_EXTERNAL ON CACHE BOOL "Google test external on")
    find_anyproject(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src)

    if(NOT BUILD_SHARED_LIBS)
        find_package(Threads)
        set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${CMAKE_THREAD_LIBS_INIT})
    endif()

    message(STATUS "target_link_libraries(${TARGET_LINK_LIB})")
    message(STATUS "link_directories(${LINK_SEARCH_PATHS})")

    link_directories(${LINK_SEARCH_PATHS})
    add_executable(main_test main_test.cpp)
    target_link_extlibraries(main_test)
    
    set_property(TARGET main_test PROPERTY CXX_STANDARD 11)
    set_property(TARGET main_test PROPERTY C_STANDARD 11)

    add_test(MainTests ${CMAKE_CURRENT_BINARY_DIR}/main_test)

endif()
