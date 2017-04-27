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
#ifndef NGSMAPVIEW_H
#define NGSMAPVIEW_H

#include "map.h"

#include "maptransform.h"

namespace ngs {

class MapView : public Map, public MapTransform
{
public:
    MapView();
    MapView(const CPLString& name, const CPLString& description,
            unsigned short epsg, const Envelope& bounds);
    virtual ~MapView() = default;
    virtual bool draw(enum ngsDrawState state, const Progress& progress = Progress()) = 0;


    // Map interface
protected:
    virtual bool openInternal(const JSONObject& root, MapFile * const mapFile) override;
    virtual bool saveInternal(JSONObject &root, MapFile * const mapFile) override;
};

typedef std::shared_ptr<MapView> MapViewPtr;

}
#endif // NGSMAPVIEW_H
