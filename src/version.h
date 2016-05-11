/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 32 of the License, or
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
 
#ifndef VERSION_H
#define VERSION_H

#define NGM_VERSION    "0.1.0"
#define NGM_VERSION_MAJOR 0
#define NGM_VERSION_MINOR 1
#define NGM_VERSION_REV   0

#define NGM_COMPUTE_VERSION(maj,min,rev) ((maj)*10000+(min)*100+rev) // maj - any, min < 99, rev < 99
#define NGM_VERSION_NUM NGM_COMPUTE_VERSION(NGM_VERSION_MAJOR,NGM_VERSION_MINOR,NGM_VERSION_REV)

/*  check if the current version is at least major.minor.revision */
#define CHECK_VERSION(major,minor,rev) \
    (NGM_VERSION_MAJOR > (major) || \
    (NGM_VERSION_MAJOR == (major) && NGM_VERSION_MINOR > (minor)) || \
    (NGM_VERSION_MAJOR == (major) && NGM_VERSION_MINOR == (minor) && \
     NGM_VERSION_REV >= (release)))

#endif // VERSION_H
