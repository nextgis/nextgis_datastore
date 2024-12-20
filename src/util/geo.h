/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2024 NextGIS, <info@nextgis.com>
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
#ifndef NGSGEO_H
#define NGSGEO_H

#include "ds/geometry.h"

#include <vector>

namespace ngs {

constexpr unsigned short MAX_TILES_COUNT = 32768; // 1.5 mb // 4096 * (4 + 4 + 1 + 8 * 4) = 164 kb

std::vector<TileItem> getTilesForExtent(
    const Envelope &extent, unsigned char zoom, bool reverseY, bool unlimitX);

} // namespace ngs


#endif // NGSGEO_H