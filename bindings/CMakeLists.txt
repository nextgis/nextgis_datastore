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

if(BUILD_BINDINGS)
    find_host_package(SWIG REQUIRED)
    include(${SWIG_USE_FILE})
    include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR})

    option(BUILD_BINDINGS_JAVA "Build JAVA bindings" OFF)
    if(BUILD_BINDINGS_JAVA)

        if(ANDROID)
            execute_process(COMMAND ${SWIG_EXECUTABLE} -java -c++
                -package ${PACKAGE_ID} -outdir ${CMAKE_CURRENT_BINARY_DIR}
                -o ${CMAKE_CURRENT_BINARY_DIR}/api_wrap.c
                ${CMAKE_CURRENT_SOURCE_DIR}/api.i)

            set(JAVA_CODE
                ${CMAKE_CURRENT_BINARY_DIR}/api.java
                ${CMAKE_CURRENT_BINARY_DIR}/apiJNI.java
            )

            install(FILES ${JAVA_CODE} DESTINATION src)
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/api_wrap.c DESTINATION src)
            # do copy of wrap file as cpp needed for some android ABI builds
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/api_wrap.c DESTINATION src
                RENAME api_wrap.cpp)
        else()
            set_source_files_properties(api.i PROPERTIES CPLUSPLUS ON)
            set(CMAKE_SWIG_FLAGS -package com.nextgis.supportlib )
            # set_source_files_properties(example.i PROPERTIES SWIG_FLAGS "-includeall")
            swig_add_module(${PROJECT_NAME}i java api.i)
            swig_link_libraries(${PROJECT_NAME}i ${TARGET_LIBS})
        endif()
    endif()
endif()    
