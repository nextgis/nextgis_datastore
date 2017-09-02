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
#include <list>
#include <memory>

// gdal
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_geometry.h"

#include "ds/featureclass.h"
#include "ds/geometry.h"
#include "map/layer.h"
#include "ngstore/codes.h"
#include "ngstore/util/constants.h"

namespace ngs {

class MapView;

class PointId
{
public:
    explicit PointId(int pointId = NOT_FOUND,
            int ringId = NOT_FOUND,
            int geometryId = NOT_FOUND) : m_pointId(pointId),
        m_ringId(ringId),
        m_geometryId(geometryId) { }

    int pointId() const { return m_pointId; }
    int ringId() const { return m_ringId; }
    int geometryId() const { return m_geometryId; }
    bool isInit() const { return 0 <= pointId(); }
    bool operator==(const PointId& other) const;

private:
    int m_pointId;
    int m_ringId; // 0 - exterior ring, 1+ - interior rings.
    int m_geometryId;
};

PointId getGeometryPointId(
        const OGRGeometry& geometry, const Envelope env, OGRPoint* coordinates);
bool shiftGeometryPoint(OGRGeometry& geometry, const PointId& id,
        const OGRRawPoint& offset, OGRPoint* coordinates);

class Overlay
{
public:
    explicit Overlay(const MapView& map, enum ngsMapOverlayType type = MOT_UNKNOWN);
    virtual ~Overlay() = default;

    enum ngsMapOverlayType type() const { return m_type; }
    bool visible() const { return m_visible; }
    virtual void setVisible(bool visible) { m_visible = visible; }

protected:
    const MapView& m_map;
    enum ngsMapOverlayType m_type;
    bool m_visible;
};

typedef std::shared_ptr<Overlay> OverlayPtr;

class EditLayerOverlay : public Overlay
{
public:
    explicit EditLayerOverlay(const MapView& map);
    virtual ~EditLayerOverlay() = default;

    virtual bool undo();
    virtual bool redo();
    bool canUndo();
    bool canRedo();
    void saveToHistory();
    void clearHistory();
    bool save();
    void cancel();

    bool createGeometry(FeatureClassPtr datasource);
    bool editGeometry();
    virtual bool addGeometryPart();
    virtual bool deleteGeometryPart();

    virtual bool selectPoint(const OGRRawPoint& mapCoordinates);
    bool hasSelectedPoint(const OGRRawPoint* mapCoordinates) const;
    virtual bool shiftPoint(const OGRRawPoint& mapOffset);

protected:
    virtual void setGeometry(GeometryUPtr geometry);
    virtual void freeResources();

private:
    bool restoreFromHistory(int historyId);
    bool selectFirstPoint();
    bool selectPoint(bool selectFirstPoint, const OGRRawPoint& mapCoordinates);

protected:
    LayerPtr m_editedLayer;
    FeatureClassPtr m_datasource;
    GIntBig m_editedFeatureId;
    GeometryUPtr m_geometry;
    PointId m_selectedPointId;
    OGRPoint m_selectedPointCoordinates;
    double m_tolerancePx;
    std::list<GeometryUPtr> m_history;
    int m_historyState;
};

class LocationOverlay : public Overlay
{
public:
    explicit LocationOverlay(const MapView& map);
    virtual ~LocationOverlay() = default;
    virtual void setLocation(const ngsCoordinate& location, float direction);

protected:
    SimplePoint m_location;
    float m_direction;
};

} // namespace ngs

#endif // NGSOVERLAY_H
