/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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

#ifndef NGSOVERLAY_H
#define NGSOVERLAY_H

// stl
#include <memory>

// gdal
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_geometry.h"

#include "ds/geometry.h"
#include "ngstore/codes.h"

#include <memory>
namespace ngs
{

class MapView;

class Overlay
{
public:
    explicit Overlay(const MapView& map, ngsMapOverlyType type = MOT_UNKNOWN);
    virtual ~Overlay() = default;

    ngsMapOverlyType type() const { return m_type; }
    bool visible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

    static int getOverlayIndexFromType(ngsMapOverlyType type)
    {
        // Overlays stored in reverse order
        switch (type) {
            case MOT_EDIT:
                return 0;
            case MOT_TRACK:
                //return 1; // TODO: Add support to track overlay
            case MOT_LOCATION:
                //return 2; // TODO: Add support to location overlay
            default:
                return -1;
        }
    }

protected:
    const MapView& m_map;
    enum ngsMapOverlyType m_type;
    bool m_visible;
};

typedef std::shared_ptr<Overlay> OverlayPtr;

class EditLayerOverlay : public Overlay
{
public:
    explicit EditLayerOverlay(const MapView& map);
    virtual ~EditLayerOverlay() = default;

    void setLayerName(const CPLString& layerName) { m_layerName = layerName; }
    const CPLString& layerName() const { return m_layerName; }
    virtual void setGeometry(GeometryPtr geometry) { m_geometry = geometry; }
    virtual GeometryPtr geometry() const { return m_geometry; }

    static GeometryPtr createGeometry(const OGRwkbGeometryType geometryType,
            const OGRRawPoint& geometryCenter);

protected:
    CPLString m_layerName;
    GeometryPtr m_geometry;
};

}  // namespace ngs

#endif  // NGSOVERLAY_H
