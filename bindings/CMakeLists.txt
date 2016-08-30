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
#    find_package(SWIG REQUIRED) # for debug
    include(${SWIG_USE_FILE})
    include_directories(${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/ds
        ${PROJECT_SOURCE_DIR}/src/map
        ${PROJECT_SOURCE_DIR}/src/util
        ${CMAKE_CURRENT_BINARY_DIR})

    option(BUILD_BINDINGS_JAVA "Build JAVA bindings" OFF)
    if(BUILD_BINDINGS_JAVA)

        if(ANDROID)
            execute_process(COMMAND ${SWIG_EXECUTABLE} -java -c++
                -package ${PACKAGE_ID} -outdir ${CMAKE_CURRENT_BINARY_DIR}
                -o ${CMAKE_CURRENT_BINARY_DIR}/api_wrap.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/api.i)

            set(CPP_CODE
                ${CMAKE_CURRENT_BINARY_DIR}/api_wrap.cpp
            )

            set(JAVA_CODE
                ${CMAKE_CURRENT_BINARY_DIR}/Api.java
                ${CMAKE_CURRENT_BINARY_DIR}/ApiJNI.java
                ${CMAKE_CURRENT_BINARY_DIR}/ChangeCodes.java
                ${CMAKE_CURRENT_BINARY_DIR}/Colour.java
                ${CMAKE_CURRENT_BINARY_DIR}/ErrorCodes.java
                ${CMAKE_CURRENT_BINARY_DIR}/NotificationCallback.java
                ${CMAKE_CURRENT_BINARY_DIR}/ProgressCallback.java
                ${CMAKE_CURRENT_BINARY_DIR}/RawEnvelope.java
                ${CMAKE_CURRENT_BINARY_DIR}/RawPoint.java
                ${CMAKE_CURRENT_BINARY_DIR}/SourceCodes.java
            )

            install(FILES ${CPP_CODE} DESTINATION src)
            install(FILES ${JAVA_CODE} DESTINATION src)
        else()
            set_source_files_properties(api.i PROPERTIES CPLUSPLUS ON)
            set(CMAKE_SWIG_FLAGS -package com.nextgis.store )
            # set_source_files_properties(example.i PROPERTIES SWIG_FLAGS "-includeall")
            swig_add_module(${PROJECT_NAME}i java api.i)
            swig_link_libraries(${PROJECT_NAME}i ${TARGET_LIBS})
        endif()
    endif()
endif()
