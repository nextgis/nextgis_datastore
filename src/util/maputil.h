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
#ifndef NGSMAPUTIL_H
#define NGSMAPUTIL_H

#include <cmath>
#include <vector>

#include "ogr_core.h"

#define LOG2 0.30102999566 //log(2.0)

namespace ngs {

typedef struct _tile {
    int x,y;
    unsigned char z;
    OGREnvelope env;
    char crossExtent;
} TileItem;

inline static double lg(double x) {return log(x) / LOG2;};
double getZoomForScale(double scale, double currentZoom);
double getPixelSize(int zoom);
std::vector<TileItem> getTilesForExtent(const OGREnvelope& extent,
                                   unsigned char zoom, bool reverseY,
                                   bool unlimitX);
}

#endif // NGSMAPUTIL_H
