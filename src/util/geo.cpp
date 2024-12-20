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
#include "geo.h"

namespace ngs {

std::vector<TileItem> getTilesForExtent(
        const Envelope &extent, unsigned char zoom, bool reverseY, bool unlimitX)
{
    Envelope env;
    std::vector<TileItem> result;
    if(zoom == 0) { // If zoom 0 - return one tile
        env = DEFAULT_BOUNDS;
        TileItem item = { {0, 0, zoom, 0}, env };
        result.push_back (item);
        return result;
    }
    int tilesInMapOneDim = 1 << zoom;
    double halfTilesInMapOneDim = tilesInMapOneDim * 0.5;
    double tilesSizeOneDim = DEFAULT_BOUNDS.maxX() / halfTilesInMapOneDim;
    int begX = static_cast<int>(std::floor(extent.minX() / tilesSizeOneDim +
                                       halfTilesInMapOneDim));
    int begY = static_cast<int>(std::floor(extent.minY() / tilesSizeOneDim +
                                       halfTilesInMapOneDim));
    int endX = static_cast<int>(std::ceil(extent.maxX() / tilesSizeOneDim +
                                      halfTilesInMapOneDim));
    int endY = static_cast<int>(std::ceil(extent.maxY() / tilesSizeOneDim +
                                      halfTilesInMapOneDim));
    if(begY == endY) {
        endY++;
    }
    if(begX == endX) {
        endX++;
    }
    if(begY < 0) {
        begY = 0;
    }
    if(endY > tilesInMapOneDim) {
        endY = tilesInMapOneDim;
    }

    // This block unlimited X scroll of the map
    if(!unlimitX) {
        if(begX < 0) {
            begX = 0;
        }
        if(endX > tilesInMapOneDim) {
            endX = tilesInMapOneDim;
        }
    }
    else {
        if(begX < -tilesInMapOneDim) {
            begX = -tilesInMapOneDim;
        }
        if(endX >= tilesInMapOneDim + tilesInMapOneDim) {
            endX = tilesInMapOneDim + tilesInMapOneDim;
        }
    }

    // Normal fill from left bottom corner
    int realX, realY;
    char crossExt;
    size_t reserveSize = static_cast<size_t>((endX - begX) * (endY - begY));
    if(reserveSize > MAX_TILES_COUNT)
        reserveSize = MAX_TILES_COUNT;
    result.reserve(reserveSize);
    double fullBoundsMinX = DEFAULT_BOUNDS.minX();
    double fullBoundsMinY = DEFAULT_BOUNDS.minY();
    for (int x = begX; x < endX; ++x) {
        realX = x;
        crossExt = 0;
        if (realX < 0) {
            crossExt = -1;
            realX += tilesInMapOneDim;
        } else if (realX >= tilesInMapOneDim) {
            crossExt = 1;
            realX -= tilesInMapOneDim;
        }
        double minX = fullBoundsMinX + realX * tilesSizeOneDim;
        env.setMinX(minX);
        env.setMaxX(minX + tilesSizeOneDim);

        for (int y = begY; y < endY; ++y) {

            if (reverseY) {
                realY = tilesInMapOneDim - y - 1;
            } else {
                realY = y;
            }

            if (realY < 0 || realY >= tilesInMapOneDim) {
                continue;
            }

            double minY = fullBoundsMinY + realY * tilesSizeOneDim;
            env.setMinY(minY);
            env.setMaxY(minY + tilesSizeOneDim);
            Tile tile = {realX, realY, zoom, crossExt};
            result.push_back( { tile, env } );

            if(result.size() > MAX_TILES_COUNT) { // Limit for tiles array size
                return result;
            }
        }
    }

    return result;
}

} // namespace ngs
