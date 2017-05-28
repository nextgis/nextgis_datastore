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
    include_directories(
        ${GTEST_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/include)

    find_package(Threads)
    set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${CMAKE_THREAD_LIBS_INIT})

    find_library(DL_LIB dl)
    set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${DL_LIB})
    find_library(M_LIB m)
    set(TARGET_LINK_LIB ${TARGET_LINK_LIB} ${M_LIB})

    #list(REMOVE_DUPLICATES TARGET_LINK_LIB)

    #message(STATUS "target_link_libraries(${TARGET_LINK_LIB})")
    #message(STATUS "link_directories(${LINK_SEARCH_PATHS})")

    link_directories(${LINK_SEARCH_PATHS})

    set(HHEADERS
        test.h
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
endif()"
    )

    add_custom_target (clean_lib_tests ALL COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/tmp" )
    add_custom_target (prepare_lib_test_data ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/data"
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/download.cmake
    )

    macro(add_ngs_test name exe_name files)
        add_executable(${exe_name} ${files} ${HHEADERS})
        target_link_extlibraries(${exe_name})
        set_target_properties(${exe_name} PROPERTIES
            CXX_STANDARD 11
            C_STANDARD 11
        )
        add_test(${name} ${CMAKE_CURRENT_BINARY_DIR}/${exe_name})
    endmacro ()

    add_ngs_test(MainTests main_test main_test.cpp)
    add_ngs_test(SettingsTests settings_test settings_test.cpp)
    add_ngs_test(MatrixTests matrix_test matrix_test.cpp)
    add_ngs_test(StorageTests store_test store_test.cpp)
    add_ngs_test(GlTests gl_test gl_test.cpp)
    add_ngs_test(MapTests map_test map_test.cpp)

endif()
