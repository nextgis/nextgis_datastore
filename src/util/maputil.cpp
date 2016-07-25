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
    int tilesInMapOneDimension = 1 << zoom;
    long sizeOneDimensionPixels = tilesInMapOneDimension * DEFAULT_TILE_SIZE;
    return DEFAULT_MAX_Y * 2.0 / sizeOneDimensionPixels;
}
