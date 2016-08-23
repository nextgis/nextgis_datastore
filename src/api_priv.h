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
#ifndef API_PRIV_H
#define API_PRIV_H

#include "api.h"
#include "ogr_geometry.h"

#include <memory>


using namespace std;

namespace ngs {
    class DataStore;
    typedef shared_ptr<DataStore> DataStorePtr;
}

ngs::DataStorePtr ngsGetCurrentDataStore();
void ngsSetCurrentDataStore(ngs::DataStorePtr ds);
/**
  * useful functions
  */
inline int ngsRGBA2HEX(const ngsRGBA& color) {
    return ((color.R & 0xff) << 24) + ((color.G & 0xff) << 16) +
            ((color.B & 0xff) << 8) + (color.A & 0xff);
}

inline ngsRGBA ngsHEX2RGBA(int color) {
    ngsRGBA out;
    out.R = (color >> 24) & 0xff;
    out.G = (color >> 16) & 0xff;
    out.B = (color >> 8) & 0xff;
    out.A = (color) & 0xff;
    return out;
}

inline OGRRawPoint ngsPointToOGRRawPoint(ngsRawPoint pt) {
    OGRRawPoint out;
    out.x = pt.x;
    out.x = pt.y;
    return out;
}

inline ngsRawPoint ogrRawPointToNgsPoint(OGRRawPoint pt) {
    ngsRawPoint out;
    out.x = pt.x;
    out.x = pt.y;
    return out;
}

inline OGREnvelope ngsEnvelopeToOGREnvelope(ngsRawEnvelope env) {
    OGREnvelope out;
    out.MinX = env.MinX;
    out.MaxX = env.MaxX;
    out.MinY = env.MinY;
    out.MaxY = env.MaxY;
    return out;
}

inline ngsRawEnvelope ogrEnvelopeToNgsEnvelope(OGREnvelope env) {
    ngsRawEnvelope out;
    out.MinX = env.MinX;
    out.MaxX = env.MaxX;
    out.MinY = env.MinY;
    out.MaxY = env.MaxY;
    return out;
}

#define ngsDynamicCast(type, shared) dynamic_cast<type*>(shared.get ())

// TODO: use gettext or something same to translate messages

#endif // API_PRIV_H
