/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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
#ifndef NGSAPI_PRIV_H
#define NGSAPI_PRIV_H

#include "ngstore/api.h"

// stl
#include <cmath>
#include <limits>
#include <memory>

//gdal
#include "cpl_string.h"

/**
  * useful functions
  */

inline std::string ngsRGBA2HEX(const ngsRGBA &color) {
    return CPLSPrintf("#%02x%02x%02x%02x", color.R, color.G, color.B, color.A);
}

inline ngsRGBA ngsHEX2RGBA(const std::string &color) {
    unsigned int r, g, b, a;
    sscanf(color.c_str(), "#%02x%02x%02x%02x", &r, &g, &b, &a);
    ngsRGBA out = {static_cast<unsigned char>(r),
                   static_cast<unsigned char>(g),
                   static_cast<unsigned char>(b),
                   static_cast<unsigned char>(a)};
    return out;
}

#define ngsDynamicCast(type, shared) dynamic_cast<type*>(shared.get ())
#define ngsStaticCast(type, shared) static_cast<type*>(shared.get ())
#define ngsUnused(x) (void)x

// http://stackoverflow.com/a/15012792
inline bool isEqual(double val1, double val2) {
    return fabs(val1 - val2) <= std::numeric_limits<double>::epsilon();
}

inline bool isEqual(float val1, float val2) {
    return fabsf(val1 - val2) <= std::numeric_limits<float>::epsilon();
}
#define ARRAY_SIZE(array) (sizeof((array))/sizeof((array[0])))

bool isDebugMode();

constexpr float M_PI_F = 3.14159265358979323846264338327950288f;
constexpr float M_PI_2_F = M_PI_F / 2.0f;
constexpr float M_PI_4_F = M_PI_F / 4.0f;

constexpr double DEG2RAD = M_PI / 180.0;
constexpr float DEG2RAD_F = M_PI_F / 180.0f;

#endif // NGSAPI_PRIV_H
