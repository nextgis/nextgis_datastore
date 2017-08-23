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

#include "overlay.h"

#include "ds/geometry.h"
#include "map/mapview.h"
#include "util/settings.h"

namespace ngs {

constexpr double TOLERANCE_PX = 7.0;
constexpr double GEOMETRY_SIZE_PX = 50.0;

Overlay::Overlay(const MapView& map, ngsMapOverlyType type)
        : m_map(map)
        , m_type(type)
        , m_visible(false)
{
}

EditLayerOverlay::EditLayerOverlay(const MapView& map)
        : Overlay(map, MOT_EDIT)
        , m_geometry(nullptr)
        , m_selectedPointId()
        , m_selectedPointCoordinates()
{
    Settings& settings = Settings::instance();
    m_tolerancePx =
            settings.getDouble("map/overlay/edit/tolerance", TOLERANCE_PX);
}

GeometryPtr EditLayerOverlay::createGeometry(
        const OGRwkbGeometryType geometryType,
        const OGRRawPoint& geometryCenter)
{
    OGRRawPoint mapDist =
            m_map.getMapDistance(GEOMETRY_SIZE_PX, GEOMETRY_SIZE_PX);

    switch(OGR_GT_Flatten(geometryType)) {
        case wkbPoint: {
            return GeometryPtr(
                    new OGRPoint(geometryCenter.x, geometryCenter.y));
        }
        case wkbLineString: {
            OGRLineString* line = new OGRLineString();
            line->addPoint(
                    geometryCenter.x - mapDist.x, geometryCenter.y - mapDist.y);
            line->addPoint(
                    geometryCenter.x + mapDist.x, geometryCenter.y + mapDist.y);
            return GeometryPtr(line);
        }
        case wkbMultiPoint: {
            OGRMultiPoint* mpt = new OGRMultiPoint();
            mpt->addGeometryDirectly(
                    new OGRPoint(geometryCenter.x, geometryCenter.y));

            // FIXME: remove it, only for test
            mpt->addGeometryDirectly(new OGRPoint(geometryCenter.x - mapDist.x,
                    geometryCenter.y - mapDist.y));
            mpt->addGeometryDirectly(new OGRPoint(geometryCenter.x + mapDist.x,
                    geometryCenter.y + mapDist.y));
            return GeometryPtr(mpt);
        }
        default: {
            return GeometryPtr();
        }
    }
}

bool EditLayerOverlay::addGeometry(const OGRRawPoint& geometryCenter)
{
    if(!m_geometry)
        return false;

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) { // only multi
        case wkbMultiPoint: {
            OGRMultiPoint* mpt = ngsDynamicCast(OGRMultiPoint, m_geometry);
            if(!mpt)
                return false;

            OGRPoint* pt = new OGRPoint(geometryCenter.x, geometryCenter.y);

            if(OGRERR_NONE != mpt->addGeometryDirectly(pt)) {
                delete pt;
                return false;
            }

            int num = mpt->getNumGeometries();
            m_selectedPointId = PointId(0, NOT_FOUND, num - 1);
            m_selectedPointCoordinates = *pt;
            return true;
        }
        default: {
            return false;
        }
    }
}

bool EditLayerOverlay::deleteGeometry()
{
    if(!m_geometry)
        return false;

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) { // only multi
        case wkbMultiPoint: {
            OGRMultiPoint* mpt = ngsDynamicCast(OGRMultiPoint, m_geometry);
            if(!mpt)
                return false;

            if(mpt->getNumGeometries() == 0)
                return false;

            if(OGRERR_NONE
                    != mpt->removeGeometry(m_selectedPointId.geometryId())) {
                return false;
            }

            int lastGeomId = mpt->getNumGeometries() - 1;
            if(lastGeomId >= 0) {
                const OGRGeometry* lastGeom = mpt->getGeometryRef(lastGeomId);
                const OGRPoint* lastPt =
                        dynamic_cast<const OGRPoint*>(lastGeom);
                m_selectedPointId = PointId(0, NOT_FOUND, lastGeomId);
                m_selectedPointCoordinates = *lastPt;
            } else {
                m_selectedPointId = PointId();
                m_selectedPointCoordinates = OGRPoint();
            }
            return true;
        }
        default: {
            return false;
        }
    }
}

void EditLayerOverlay::setGeometry(GeometryPtr geometry)
{
    m_geometry = geometry;
    selectFirstPoint();
}

bool EditLayerOverlay::selectPoint(const OGRRawPoint& mapCoordinates)
{
    return selectPoint(false, mapCoordinates);
}

bool EditLayerOverlay::selectFirstPoint()
{
    return selectPoint(true, OGRRawPoint());
}

bool EditLayerOverlay::selectPoint(
        bool selectFirstPoint, const OGRRawPoint& mapCoordinates)
{
    if(m_geometry) {
        OGRPoint coordinates;
        PointId id;

        if(selectFirstPoint) {
            id = getGeometryPointId(*m_geometry, DEFAULT_BOUNDS, &coordinates);
        } else {
            OGRRawPoint mapTolerance =
                    m_map.getMapDistance(m_tolerancePx, m_tolerancePx);

            double minX = mapCoordinates.x - mapTolerance.x;
            double maxX = mapCoordinates.x + mapTolerance.x;
            double minY = mapCoordinates.y - mapTolerance.y;
            double maxY = mapCoordinates.y + mapTolerance.y;
            Envelope mapEnv(minX, minY, maxX, maxY);

            id = getGeometryPointId(*m_geometry, mapEnv, &coordinates);
        }

        if(id.isInit()) {
            m_selectedPointId = id;
            m_selectedPointCoordinates = coordinates;
            return true;
        }
    }
    return false;
}

bool EditLayerOverlay::hasSelectedPoint(const OGRRawPoint* mapCoordinates) const
{
    bool ret = m_selectedPointId.isInit();
    if(ret && mapCoordinates) {
        OGRRawPoint mapTolerance =
                m_map.getMapDistance(m_tolerancePx, m_tolerancePx);

        double minX = mapCoordinates->x - mapTolerance.x;
        double maxX = mapCoordinates->x + mapTolerance.x;
        double minY = mapCoordinates->y - mapTolerance.y;
        double maxY = mapCoordinates->y + mapTolerance.y;
        Envelope mapEnv(minX, minY, maxX, maxY);

        ret = geometryIntersects(m_selectedPointCoordinates, mapEnv);
    }
    return ret;
}

bool EditLayerOverlay::shiftPoint(const OGRRawPoint& mapOffset)
{
    if(!m_geometry || !m_selectedPointId.isInit()) {
        return false;
    }

    return shiftGeometryPoint(*m_geometry, m_selectedPointId, mapOffset,
            &m_selectedPointCoordinates);
}

bool PointId::operator==(const PointId& other) const
{
    return m_pointId == other.m_pointId && m_ringId == other.m_ringId
            && m_geometryId == other.m_geometryId;
}

PointId getPointId(
        const OGRPoint& pt, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(pt, env)) {
        return PointId();
    }

    if(coordinates) {
        coordinates->setX(pt.getX());
        coordinates->setY(pt.getY());
    }
    return PointId(0);
}

PointId getLineStringPointId(
        const OGRLineString& line, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(line, env)) {
        return PointId();
    }

    int id = 0;
    bool found = false;
    OGRPointIterator* it = line.getPointIterator();
    OGRPoint pt;
    while(it->getNextPoint(&pt)) {
        if(geometryIntersects(pt, env)) {
            found = true;
            break;
        }
        ++id;
    }
    OGRPointIterator::destroy(it);

    if(found) {
        if(coordinates) {
            coordinates->setX(pt.getX());
            coordinates->setY(pt.getY());
        }
        return PointId(id);
    } else {
        return PointId();
    }
}

PointId getPolygonPointId(
        const OGRPolygon& polygon, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(polygon, env)) {
        return PointId();
    }

    const OGRLinearRing* ring = polygon.getExteriorRing();
    int numInteriorRings = polygon.getNumInteriorRings();
    int pointId = 0;
    int ringId = 0;

    while(true) {
        if(!ring) {
            return PointId();
        }

        PointId id = getLineStringPointId(*ring, env, coordinates);
        if(id.isInit()) {
            return PointId(id.pointId(), ringId);
        }

        if(ringId >= numInteriorRings) {
            break;
        }

        ring = polygon.getInteriorRing(ringId++);
    }

    return PointId();
}

PointId getMultiPointPointId(
        const OGRMultiPoint& mpt, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(mpt, env)) {
        return PointId();
    }

    for(int geometryId = 0, num = mpt.getNumGeometries(); geometryId < num;
            ++geometryId) {
        const OGRPoint* pt =
                static_cast<const OGRPoint*>(mpt.getGeometryRef(geometryId));
        if(geometryIntersects(*pt, env)) {
            if(coordinates) {
                coordinates->setX(pt->getX());
                coordinates->setY(pt->getY());
            }
            return PointId(0, NOT_FOUND, geometryId);
        }
    }

    return PointId();
}

PointId getMultiLineStringPointId(const OGRMultiLineString& mline,
        const Envelope env,
        OGRPoint* coordinates)
{
    if(!geometryIntersects(mline, env)) {
        return PointId();
    }

    for(int geometryId = 0, numGeoms = mline.getNumGeometries();
            geometryId < numGeoms; ++geometryId) {

        const OGRLineString* line = static_cast<const OGRLineString*>(
                mline.getGeometryRef(geometryId));

        PointId id = getLineStringPointId(*line, env, coordinates);
        if(id.isInit()) {
            return PointId(id.pointId(), NOT_FOUND, geometryId);
        }
    }

    return PointId();
}

PointId getMultiPolygonPointId(const OGRMultiPolygon& mpolygon,
        const Envelope env,
        OGRPoint* coordinates)
{
    if(!geometryIntersects(mpolygon, env)) {
        return PointId();
    }

    for(int geometryId = 0, numGeoms = mpolygon.getNumGeometries();
            geometryId < numGeoms; ++geometryId) {

        const OGRPolygon* polygon = static_cast<const OGRPolygon*>(
                mpolygon.getGeometryRef(geometryId));
        PointId id = getPolygonPointId(*polygon, env, coordinates);
        if(id.isInit()) {
            return PointId(id.pointId(), id.ringId(), geometryId);
        }
    }

    return PointId();
}

PointId getGeometryPointId(
        const OGRGeometry& geometry, const Envelope env, OGRPoint* coordinates)
{
    switch(OGR_GT_Flatten(geometry.getGeometryType())) {
        case wkbPoint: {
            const OGRPoint& pt = static_cast<const OGRPoint&>(geometry);
            return getPointId(pt, env, coordinates);
        }
        case wkbLineString: {
            const OGRLineString& lineString =
                    static_cast<const OGRLineString&>(geometry);
            return getLineStringPointId(lineString, env, coordinates);
        }
        case wkbPolygon: {
            const OGRPolygon& polygon =
                    static_cast<const OGRPolygon&>(geometry);
            return getPolygonPointId(polygon, env, coordinates);
        }
        case wkbMultiPoint: {
            const OGRMultiPoint& mpt =
                    static_cast<const OGRMultiPoint&>(geometry);
            return getMultiPointPointId(mpt, env, coordinates);
        }
        case wkbMultiLineString: {
            const OGRMultiLineString& mline =
                    static_cast<const OGRMultiLineString&>(geometry);
            return getMultiLineStringPointId(mline, env, coordinates);
        }
        case wkbMultiPolygon: {
            const OGRMultiPolygon& mpolygon =
                    static_cast<const OGRMultiPolygon&>(geometry);
            return getMultiPolygonPointId(mpolygon, env, coordinates);
        }
    }
    return PointId();
}

bool shiftPoint(OGRPoint& pt,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    if(0 != id.pointId()) {
        return false;
    }

    pt.setX(pt.getX() + offset.x);
    pt.setY(pt.getY() + offset.y);
    if(coordinates) {
        coordinates->setX(pt.getX());
        coordinates->setY(pt.getY());
    }
    return true;
}

bool shiftLineStringPoint(OGRLineString& lineString,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    if(pointId < 0 || pointId >= lineString.getNumPoints()) {
        return false;
    }

    OGRPoint pt;
    lineString.getPoint(pointId, &pt);
    lineString.setPoint(pointId, pt.getX() + offset.x, pt.getY() + offset.y);
    if(coordinates) {
        coordinates->setX(pt.getX());
        coordinates->setY(pt.getY());
    }
    return true;
}

bool shiftPolygonPoint(OGRPolygon& polygon,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    int ringId = id.ringId();
    int numInteriorRings = polygon.getNumInteriorRings();

    // ringId == 0 - exterior ring, 1+ - interior rings
    if(pointId < 0 || ringId < 0 || ringId > numInteriorRings) {
        return false;
    }

    OGRLinearRing* ring = (0 == ringId) ? polygon.getExteriorRing()
                                        : polygon.getInteriorRing(ringId - 1);
    if(!ring) {
        return false;
    }

    int ringNumPoints = ring->getNumPoints();
    if(pointId >= ringNumPoints) {
        return false;
    }

    return shiftLineStringPoint(*ring, PointId(pointId), offset, coordinates);
}

bool shiftMultiPointPoint(OGRMultiPoint& mpt,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    int geometryId = id.geometryId();
    if(pointId != 0 || geometryId < 0 || geometryId >= mpt.getNumGeometries()) {
        return false;
    }
    OGRPoint* pt = static_cast<OGRPoint*>(mpt.getGeometryRef(geometryId));
    return shiftPoint(*pt, PointId(0), offset, coordinates);
}

bool shiftMultiLineStringPoint(OGRMultiLineString& mline,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    int geometryId = id.geometryId();
    if(pointId < 0 || geometryId < 0
            || geometryId >= mline.getNumGeometries()) {
        return false;
    }

    OGRLineString* line =
            static_cast<OGRLineString*>(mline.getGeometryRef(geometryId));

    int lineNumPoints = line->getNumPoints();
    if(pointId >= lineNumPoints) {
        return false;
    }

    return shiftLineStringPoint(*line, PointId(pointId), offset, coordinates);
}

bool shiftMultiPolygonPoint(OGRMultiPolygon& mpolygon,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    int ringId = id.ringId();
    int geometryId = id.geometryId();
    if(pointId < 0 || ringId < 0 || geometryId < 0
            || geometryId >= mpolygon.getNumGeometries()) {
        return false;
    }

    OGRPolygon* polygon =
            static_cast<OGRPolygon*>(mpolygon.getGeometryRef(geometryId));

    return shiftPolygonPoint(
            *polygon, PointId(pointId, ringId), offset, coordinates);
}

bool shiftGeometryPoint(OGRGeometry& geometry,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    switch(OGR_GT_Flatten(geometry.getGeometryType())) {
        case wkbPoint: {
            OGRPoint& pt = static_cast<OGRPoint&>(geometry);
            return shiftPoint(pt, id, offset, coordinates);
        }
        case wkbLineString: {
            OGRLineString& lineString = static_cast<OGRLineString&>(geometry);
            return shiftLineStringPoint(lineString, id, offset, coordinates);
        }
        case wkbPolygon: {
            OGRPolygon& polygon = static_cast<OGRPolygon&>(geometry);
            return shiftPolygonPoint(polygon, id, offset, coordinates);
        }
        case wkbMultiPoint: {
            OGRMultiPoint& mpt = static_cast<OGRMultiPoint&>(geometry);
            return shiftMultiPointPoint(mpt, id, offset, coordinates);
        }
        case wkbMultiLineString: {
            OGRMultiLineString& mline =
                    static_cast<OGRMultiLineString&>(geometry);
            return shiftMultiLineStringPoint(mline, id, offset, coordinates);
        }
        case wkbMultiPolygon: {
            OGRMultiPolygon& mpolygon = static_cast<OGRMultiPolygon&>(geometry);
            return shiftMultiPolygonPoint(mpolygon, id, offset, coordinates);
        }
        default: {
            return false;
        }
    }
}

} // namespace ngs
