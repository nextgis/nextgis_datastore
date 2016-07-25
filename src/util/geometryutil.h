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
#ifndef GEOMETRYUTIL_H
#define GEOMETRYUTIL_H

#include "maputil.h"

#include <array>

#include "ogrsf_frmts.h"

#define SAMPLE_DISTANCE_PX 5

namespace ngs {

using namespace std;

const static array<pair<double, char>, 4> sampleDists = {
{{ getPixelSize(6)  * SAMPLE_DISTANCE_PX, 6 },
 { getPixelSize(9)  * SAMPLE_DISTANCE_PX, 9 },
 { getPixelSize(12) * SAMPLE_DISTANCE_PX, 12 },
 { getPixelSize(15) * SAMPLE_DISTANCE_PX, 15 }
}};

OGRGeometry* simplifyGeometry(const OGRGeometry* geometry, double distance);

}

#endif // GEOMETRYUTIL_H
