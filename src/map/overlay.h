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
            int geometryId = NOT_FOUND);
    ~PointId() = default;

    bool isValid() const { return 0 <= pointId(); }
    bool operator==(const PointId& other) const;

    void setPointId(int pointId) { m_pointId = pointId; }
    int pointId() const { return m_pointId; }
    void setRingId(int ringId) { m_ringId = ringId; }
    int ringId() const { return m_ringId; }
    void setGeometryId(int geometryId) { m_geometryId = geometryId; }
    int geometryId() const { return m_geometryId; }

    const PointId& setIntersects();
    bool intersects() const { return m_ringId >= 0 || m_geometryId >= 0; }

    static PointId getGeometryPointId(const OGRGeometry& geometry,
            const Envelope env,
            const PointId* selectedPointId = nullptr,
            OGRPoint* coordinates = nullptr);
    static PointId getLineStringMedianPointId(const OGRLineString& line,
            const Envelope env,
            OGRPoint* coordinates = nullptr);
    static OGRPoint getGeometryPointCoordinates(
            const OGRGeometry& geometry, const PointId& id);
    static bool shiftGeometryPoint(OGRGeometry& geometry,
            const PointId& id,
            const OGRRawPoint& offset,
            OGRPoint* coordinates = nullptr);

private:
    static PointId getPointId(const OGRPoint& pt,
            const Envelope env,
            const PointId* selectedPointId,
            OGRPoint* coordinates);
    static PointId getLineStringPointId(const OGRLineString& line,
            const Envelope env,
            const PointId* selectedPointId,
            OGRPoint* coordinates);
    static PointId getPolygonPointId(const OGRPolygon& polygon,
            const Envelope env,
            const PointId* selectedPointId,
            OGRPoint* coordinates);
    static PointId getMultiPointPointId(const OGRMultiPoint& mpt,
            const Envelope env,
            const PointId* selectedPointId,
            OGRPoint* coordinates);
    static PointId getMultiLineStringPointId(const OGRMultiLineString& mline,
            const Envelope env,
            const PointId* selectedPointId,
            OGRPoint* coordinates);
    static PointId getMultiPolygonPointId(const OGRMultiPolygon& mpolygon,
            const Envelope env,
            const PointId* selectedPointId,
            OGRPoint* coordinates);

    static OGRPoint getPointCoordinates(const OGRPoint& pt, const PointId& id);
    static OGRPoint getLineStringPointCoordinates(
            const OGRLineString& line, const PointId& id);
    static OGRPoint getPolygonPointCoordinates(
            const OGRPolygon& polygon, const PointId& id);
    static OGRPoint getMultiPointPointCoordinates(
            const OGRMultiPoint& mpt, const PointId& id);
    static OGRPoint getMultiLineStringPointCoordinates(
            const OGRMultiLineString& mline, const PointId& id);
    static OGRPoint getMultiPolygonPointCoordinates(
            const OGRMultiPolygon& mpolygon, const PointId& id);

    static bool shiftPoint(OGRPoint& pt,
            const PointId& id,
            const OGRRawPoint& offset,
            OGRPoint* coordinates);
    static bool shiftLineStringPoint(OGRLineString& lineString,
            const PointId& id,
            const OGRRawPoint& offset,
            OGRPoint* coordinates);
    static bool shiftPolygonPoint(OGRPolygon& polygon,
            const PointId& id,
            const OGRRawPoint& offset,
            OGRPoint* coordinates);
    static bool shiftMultiPointPoint(OGRMultiPoint& mpt,
            const PointId& id,
            const OGRRawPoint& offset,
            OGRPoint* coordinates);
    static bool shiftMultiLineStringPoint(OGRMultiLineString& mline,
            const PointId& id,
            const OGRRawPoint& offset,
            OGRPoint* coordinates);
    static bool shiftMultiPolygonPoint(OGRMultiPolygon& mpolygon,
            const PointId& id,
            const OGRRawPoint& offset,
            OGRPoint* coordinates);

private:
    int m_pointId;
    int m_ringId; // For polygon: 0 - exterior ring, 1+ - interior rings.
    int m_geometryId;
};

class Overlay
{
public:
    explicit Overlay(MapView* map, enum ngsMapOverlayType type = MOT_UNKNOWN);
    virtual ~Overlay() = default;

    enum ngsMapOverlayType type() const { return m_type; }
    bool visible() const { return m_visible; }
    virtual void setVisible(bool visible) { m_visible = visible; }

protected:
    MapView* m_map;
    enum ngsMapOverlayType m_type;
    bool m_visible;
};

typedef std::shared_ptr<Overlay> OverlayPtr;

class EditLayerOverlay : public Overlay
{
public:
    explicit EditLayerOverlay(MapView* map);
    virtual ~EditLayerOverlay() = default;

    virtual bool undo();
    virtual bool redo();
    bool canUndo();
    bool canRedo();
    void saveToHistory();
    void clearHistory();
    FeaturePtr save();
    void cancel();

    bool createGeometry(FeatureClassPtr datasource);
    bool editGeometry(LayerPtr layer, GIntBig featureId);
    bool deleteGeometry();
    virtual bool addPoint();
    virtual bool deletePoint();
    virtual bool addGeometryPart();
    virtual bool deleteGeometryPart();

    ngsPointId touch(double x, double y, enum ngsMapTouchType type);

protected:
    bool hasSelectedPoint(const OGRRawPoint& mapCoordinates) const;
    virtual bool singleTap(const OGRRawPoint& mapCoordinates);
    virtual bool shiftPoint(const OGRRawPoint& mapOffset);
    virtual void setGeometry(GeometryUPtr geometry);
    virtual void freeResources();

private:
    bool restoreFromHistory(int historyId);
    int addPoint(OGRLineString* line, int id, const OGRPoint& pt);
    bool clickPoint(const OGRRawPoint& mapCoordinates);
    bool clickMedianPoint(const OGRRawPoint& mapCoordinates);
    bool clickLine(const OGRRawPoint& mapCoordinates);
    bool selectFirstPoint();

protected:
    LayerPtr m_editLayer;
    FeatureClassPtr m_featureClass;
    GIntBig m_editFeatureId;
    GeometryUPtr m_geometry;
    PointId m_selectedPointId;
    OGRPoint m_selectedPointCoordinates;
    double m_tolerancePx;
    std::list<GeometryUPtr> m_history;
    int m_historyState;

    OGRRawPoint m_touchStartPoint;
    bool m_isTouchMoved;
    bool m_isTouchingSelectedPoint;
};

class LocationOverlay : public Overlay
{
public:
    explicit LocationOverlay(MapView* map);
    virtual ~LocationOverlay() = default;
    virtual void setLocation(const ngsCoordinate& location, float direction,
                             float accuracy);

protected:
    SimplePoint m_location;
    float m_direction, m_accuracy;
};

} // namespace ngs

#endif // NGSOVERLAY_H
