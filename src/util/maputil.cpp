/******************************************************************************
*  Project: NextGIS ...
*  Purpose: Application for ...
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2012-2016 NextGIS, info@nextgis.ru
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include "maputil.h"
#include "constants.h"

#define MAX_TILES_COUNT 4096 // 4096 * (4 + 4 + 1 + 8 * 4) = 164 kb

double ngs::getZoomForScale(double scale, double currentZoom)
{
    double zoom = currentZoom;
    if (scale > 1) {
        zoom = currentZoom + lg(scale);
    } else if (scale < 1) {
        zoom = currentZoom - lg(1.0 / scale);
    }
    return zoom;
}

double ngs::getPixelSize(int zoom)
{
    int tilesInMapOneDim = 1 << zoom;
    long sizeOneDimPixels = tilesInMapOneDim * DEFAULT_TILE_SIZE;
    return DEFAULT_MAX_X * 2.0 / sizeOneDimPixels;
}

vector<ngs::TileItem> ngs::getTilesForExtent(const OGREnvelope &extent,
                                             unsigned char zoom, bool reverseY,
                                             bool unlimitX)
{
    OGREnvelope env;
    vector<ngs::TileItem> result;
    if(zoom == 0) {
        env.MinX = DEFAULT_MIN_X;
        env.MaxX = DEFAULT_MAX_X;
        env.MinY = DEFAULT_MIN_Y;
        env.MaxY = DEFAULT_MAX_Y;
        TileItem item = { 0, 0, zoom, env, 0 };
        result.push_back (item);
        return result;
    }
    int tilesInMapOneDim = 1 << zoom;
    double halfTilesInMapOneDim = tilesInMapOneDim * 0.5;
    double tilesSizeOneDim = DEFAULT_MAX_X / halfTilesInMapOneDim;
    int begX = static_cast<int>( floor(extent.MinX / tilesSizeOneDim +
                                       halfTilesInMapOneDim) );
    int begY = static_cast<int>( floor(extent.MinY / tilesSizeOneDim +
                                       halfTilesInMapOneDim) );
    int endX = static_cast<int>( ceil(extent.MaxX / tilesSizeOneDim +
                                      halfTilesInMapOneDim) ) + 1;
    int endY = static_cast<int>( ceil(extent.MaxY / tilesSizeOneDim +
                                      halfTilesInMapOneDim) ) + 1;
    if(begY == endY)
        endY++;
    if(begX == endX)
        endX++;

    if (begY < 0) {
        begY = 0;
    }
    if (endY > tilesInMapOneDim) {
        endY = tilesInMapOneDim;
    }
    /* this block unlimited X scroll of the map*/
    if(!unlimitX) {
        if (begX < 0) {
            begX = 0;
        }
        if (endX > tilesInMapOneDim) {
            endX = tilesInMapOneDim;
        }
    }

    // TODO: fill by spiral

    // normal fill from left bottom corner

    int realX, realY;
    char crossExt;
    result.reserve ( (endX - begX) * (endY - begY) );
    double fullBoundsMinX = DEFAULT_MIN_X;
    double fullBoundsMinY = DEFAULT_MIN_Y;
    for (int x = begX; x < endX; x++) {
        for (int y = begY; y < endY; y++) {
            realX = x;            
            crossExt = 0;
            if (realX < 0) {
                crossExt = -1;
                realX += tilesInMapOneDim;
            } else if (realX >= tilesInMapOneDim) {
                crossExt = 1;
                realX -= tilesInMapOneDim;
            }

            realY = y;
            if (reverseY) {
                realY = tilesInMapOneDim - y - 1;
            }

            if (realY < 0 || realY >= tilesInMapOneDim) {
                continue;
            }

            double minX = fullBoundsMinX + realX * tilesSizeOneDim;
            double minY = fullBoundsMinY + realY * tilesSizeOneDim;
            env.MinX = minX;
            env.MaxX = minX + tilesSizeOneDim;
            env.MinY = minY;
            env.MaxY = minY + tilesSizeOneDim;
            TileItem item = { realX, realY, zoom, env, crossExt };
            result.push_back (item);

            if(result.size() > MAX_TILES_COUNT) // some limits for tiles array size
                return result;
        }
    }

    return result;
}
