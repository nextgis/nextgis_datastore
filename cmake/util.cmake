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

function(check_version major minor rev)

    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h VERSION_H_CONTENTS)

    string(REGEX MATCH "NGM_VERSION_MAJOR[ \t]+([0-9]+)"
      MAJOR_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      MAJOR_VERSION ${MAJOR_VERSION})
    string(REGEX MATCH "NGM_VERSION_MINOR[ \t]+([0-9]+)"
      MINOR_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      MINOR_VERSION ${MINOR_VERSION})
    string(REGEX MATCH "NGM_VERSION_REV[ \t]+([0-9]+)"
      REV_VERSION ${VERSION_H_CONTENTS})
    string (REGEX MATCH "([0-9]+)"
      REV_VERSION ${REV_VERSION})
    
    set(${major} ${MAJOR_VERSION} PARENT_SCOPE)
    set(${minor} ${MINOR_VERSION} PARENT_SCOPE)
    set(${rev} ${REV_VERSION} PARENT_SCOPE)

endfunction(check_version)


function(report_version name ver)

    string(ASCII 27 Esc)
    set(BoldYellow  "${Esc}[1;33m")
    set(ColourReset "${Esc}[m")
        
    message(STATUS "${BoldYellow}${name} version ${ver}${ColourReset}")
    
endfunction()  
