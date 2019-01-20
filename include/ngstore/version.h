/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/

#ifndef NGSVERSION_H
#define NGSVERSION_H

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define NGS_VERSION_MAJOR 0
#define NGS_VERSION_MINOR 8
#define NGS_VERSION_REV   0
#define NGS_VERSION  STR(NGS_VERSION_MAJOR) "." STR(NGS_VERSION_MINOR) "." \
    STR(NGS_VERSION_REV)

#ifndef NGS_ABI
#define NGS_ABI "Unknown"
#endif

#ifndef NGS_USERAGENT
#define NGS_USERAGENT "Next.GIS store library " NGS_VERSION " [" NGS_ABI "]"
#endif

#define NGS_COMPUTE_VERSION(maj,min,rev) ((maj)*10000+(min)*100+rev) // major - any, min < 99, rev < 99
#define NGS_VERSION_NUM NGS_COMPUTE_VERSION(NGS_VERSION_MAJOR,NGS_VERSION_MINOR,NGS_VERSION_REV)

/*  check if the current version is at least major.minor.revision */
#define CHECK_VERSION(major,minor,rev) \
    (NGS_VERSION_MAJOR > (major) || \
    (NGS_VERSION_MAJOR == (major) && NGS_VERSION_MINOR > (minor)) || \
    (NGS_VERSION_MAJOR == (major) && NGS_VERSION_MINOR == (minor) && \
     NGS_VERSION_REV >= (release)))

#endif // NGSVERSION_H
