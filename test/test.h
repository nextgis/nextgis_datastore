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
#ifndef NGSTEST_H
#define NGSTEST_H

#include "gtest/gtest.h"

#include "ngstore/api.h"

//#define TMS_URL "http://tile2.maps.2gis.com/tiles?x={x}&y={y}&z={z}&v=1.1"
//#define TMS_NAME "2gis"
//#define TMS_ALIAS "maps.2gis.com"
//#define TMS_COPYING "2gis (c)"
//#define TMS_EPSG 3857
//#define TMS_MIN_Z 0
//#define TMS_MAX_Z 18
//#define TMS_YORIG_TOP false

void initLib();
void ngsTestNotifyFunc(const char* /*uri*/, enum ngsChangeCode /*operation*/);
int ngsTestProgressFunc(enum ngsCode /*status*/, double /*complete*/,
                        const char* /*message*/, void* /*progressArguments*/);
int ngsGDALProgressFunc(double /*dfComplete*/, const char */*pszMessage*/,
                        void */*pProgressArg*/);
void resetCounter();
int getCounter();
CatalogObjectH createConnection(const std::string &url);
CatalogObjectH createGroup(CatalogObjectH parent, const std::string &name);
CatalogObjectH getLocalFile(const std::string &name);
CatalogObjectH createMIStore(const std::string &name);

#endif // NGSTEST_H
