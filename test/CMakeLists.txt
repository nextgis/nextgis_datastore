################################################################################
#  Project: libngstore
#  Purpose: NextGIS store and visualisation support library
#  Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
#  Language: C/C++
################################################################################
#  GNU Lesser General Public Licens v3
#
#  Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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
    include_directories(
        ${GTEST_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include)

    link_directories(${LINK_SEARCH_PATHS})

    set(HHEADERS
        test.h
    )

    set(CSOURCES
        test.cpp
    )

    set(TEST_DATA_URL "https://raw.githubusercontent.com/nextgis/testdata/master")

    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/download.cmake
"if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/data/bld.shp)
file(DOWNLOAD ${TEST_DATA_URL}/vector/shape/bld.shp ${CMAKE_CURRENT_BINARY_DIR}/data/bld.shp)
file(DOWNLOAD ${TEST_DATA_URL}/vector/shape/bld.cpg ${CMAKE_CURRENT_BINARY_DIR}/data/bld.cpg)
file(DOWNLOAD ${TEST_DATA_URL}/vector/shape/bld.dbf ${CMAKE_CURRENT_BINARY_DIR}/data/bld.dbf)
file(DOWNLOAD ${TEST_DATA_URL}/vector/shape/bld.prj ${CMAKE_CURRENT_BINARY_DIR}/data/bld.prj)
file(DOWNLOAD ${TEST_DATA_URL}/vector/shape/bld.shx ${CMAKE_CURRENT_BINARY_DIR}/data/bld.shx)
endif()
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/data/railway.zip)
file(DOWNLOAD ${TEST_DATA_URL}/vector/shape/railway.zip ${CMAKE_CURRENT_BINARY_DIR}/data/railway.zip)
endif()
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/data/railway-mini.zip)
file(DOWNLOAD ${TEST_DATA_URL}/vector/shape/railway-mini.zip ${CMAKE_CURRENT_BINARY_DIR}/data/railway-mini.zip)
endif()
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/data/tex.png)
file(DOWNLOAD ${TEST_DATA_URL}/raster/png/tex.png ${CMAKE_CURRENT_BINARY_DIR}/data/tex.png)
endif()
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/data/rgbsmall.tif)
file(DOWNLOAD ${TEST_DATA_URL}/raster/geotiff/rgbsmall.tif ${CMAKE_CURRENT_BINARY_DIR}/data/rgbsmall.tif)
file(DOWNLOAD ${TEST_DATA_URL}/style/rgbsmall.qml ${CMAKE_CURRENT_BINARY_DIR}/data/rgbsmall.qml)
endif()
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/data/bld.tab)
file(DOWNLOAD ${TEST_DATA_URL}/vector/mapinfo/bld.tab ${CMAKE_CURRENT_BINARY_DIR}/data/bld.tab)
file(DOWNLOAD ${TEST_DATA_URL}/vector/mapinfo/bld.dat ${CMAKE_CURRENT_BINARY_DIR}/data/bld.dat)
file(DOWNLOAD ${TEST_DATA_URL}/vector/mapinfo/bld.map ${CMAKE_CURRENT_BINARY_DIR}/data/bld.map)
file(DOWNLOAD ${TEST_DATA_URL}/vector/mapinfo/bld.id ${CMAKE_CURRENT_BINARY_DIR}/data/bld.id)
endif()"
    )

    add_custom_target (clean_lib_tests ALL COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/tmp" )
    add_custom_target (prepare_lib_test_data ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/download.cmake
    )

    macro(add_ngs_test name exe_name files)
        add_executable(${exe_name} ${files} ${CSOURCES} ${HHEADERS})
        target_link_extlibraries(${exe_name})
        set_target_properties(${exe_name} PROPERTIES
            CXX_STANDARD 11
            C_STANDARD 11
        )

        if(APPLE)
            # For debug
            add_custom_command(TARGET ${exe_name}
                POST_BUILD COMMAND
                ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "~/Applications/NextGIS/Library/Frameworks/"
                $<TARGET_FILE:${exe_name}>)
        endif()

        add_test(NAME ${name} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${exe_name})
    endmacro ()

    add_ngs_test(MainTests main_test main_test.cpp)
    add_ngs_test(SettingsTests settings_test settings_test.cpp)
    add_ngs_test(StorageTests store_test store_test.cpp)
    if(UNIX)
	add_ngs_test(GlTests gl_test gl_test.cpp)
	add_ngs_test(MapTests map_test map_test.cpp)
    endif()
    add_ngs_test(NGWTests ngw_test ngw_test.cpp)

endif()
