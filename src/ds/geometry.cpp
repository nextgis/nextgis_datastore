/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#include "geometry.h"

#include "api_priv.h"
#include "ngstore/util/constants.h"


namespace ngs {

constexpr double BIG_VALUE = 10000000.0;
constexpr double DBLNAN = 0.0;

constexpr const char* MAP_MIN_X_KEY = "min_x";
constexpr const char* MAP_MIN_Y_KEY = "min_y";
constexpr const char* MAP_MAX_X_KEY = "max_x";
constexpr const char* MAP_MAX_Y_KEY = "max_y";

Envelope::Envelope() :
    m_minX(DBLNAN),
    m_minY(DBLNAN),
    m_maxX(DBLNAN),
    m_maxY(DBLNAN)
{

}

void Envelope::operator=(const OGREnvelope &env)
{
    m_minX = env.MinX;
    m_minY = env.MinY;
    m_maxX = env.MaxX;
    m_maxY = env.MaxY;
}

void Envelope::clear()
{
    m_minX = DBLNAN;
    m_minY = DBLNAN;
    m_maxX = DBLNAN;
    m_maxY = DBLNAN;
}

bool Envelope::isInit() const
{
    return !isEqual(m_minX, DBLNAN) || !isEqual(m_minY, DBLNAN) ||
           !isEqual(m_maxX, DBLNAN) || !isEqual(m_maxY, DBLNAN);
}

OGRRawPoint Envelope::center() const
{
    OGRRawPoint pt;
    pt.x = m_minX + width() * .5;
    pt.y = m_minY + height() * .5;
    return pt;
}

void Envelope::rotate(double angle)
{
    double cosA = cos(angle);
    double sinA = sin(angle);

    OGRRawPoint points[4];
    points[0] = OGRRawPoint(m_minX, m_minY);
    points[1] = OGRRawPoint(m_maxX, m_minY);
    points[2] = OGRRawPoint(m_maxX, m_maxY);
    points[3] = OGRRawPoint(m_minX, m_maxY);

    m_minX = BIG_VALUE;
    m_minY = BIG_VALUE;
    m_maxX = -BIG_VALUE;
    m_maxY = -BIG_VALUE;

    double x, y;
    for(OGRRawPoint &pt : points){
        x = pt.x * cosA - pt.y * sinA;
        y = pt.x * sinA + pt.y * cosA;

        if(x < m_minX)
            m_minX = x;
        if(x > m_maxX)
            m_maxX = x;

        if(y < m_minY)
            m_minY = y;
        if(y > m_maxY)
            m_maxY = y;
    }
}

void Envelope::setRatio(double ratio)
{
    double halfWidth = width() * .5;
    double halfHeight = height() * .5;
    OGRRawPoint center(m_minX + halfWidth,  m_minY + halfHeight);
    double envRatio = halfWidth / halfHeight;
    if(isEqual(envRatio, ratio))
        return;
    if(ratio > envRatio) //increase width
    {
        double width = halfHeight * ratio;
        m_maxX = center.x + width;
        m_minX = center.x - width;
    }
    else					//increase height
    {
        double height = halfWidth / ratio;
        m_maxY = center.y + height;
        m_minY = center.y - height;
    }
}

void Envelope::resize(double value)
{
    if(isEqual(value, 1.0))
        return;
    double w = width() * .5;
    double h = height() * .5;
    double x = m_minX + w;
    double y = m_minY + h;

    w *= value;
    h *= value;

    m_minX = x - w;
    m_maxX = x + w;
    m_minY = y - h;
    m_maxY = y + h;
}

void Envelope::move(double deltaX, double deltaY)
{
    m_minX += deltaX;
    m_maxX += deltaX;
    m_minY += deltaY;
    m_maxY += deltaY;
}

GeometryPtr Envelope::toGeometry(OGRSpatialReference * const spatialRef) const
{
    if(!isInit())
        return GeometryPtr();
    OGRLinearRing ring;
    ring.addPoint(m_minX, m_minY);
    ring.addPoint(m_minX, m_maxY);
    ring.addPoint(m_maxX, m_maxY);
    ring.addPoint(m_maxX, m_minY);
    ring.closeRings();

    OGRPolygon* rgn = new OGRPolygon();
    rgn->addRing(&ring);
    rgn->flattenTo2D();
    if(nullptr != spatialRef) {
        spatialRef->Reference();
        rgn->assignSpatialReference(spatialRef);
    }
    return GeometryPtr(static_cast<OGRGeometry*>(rgn));
}

OGREnvelope Envelope::toOgrEnvelope() const
{
    OGREnvelope env;
    env.MaxX = m_maxX;
    env.MaxY = m_maxY;
    env.MinX = m_minX;
    env.MinY = m_minY;
    return env;
}

bool Envelope::load(const CPLJSONObject &store, const Envelope& defaultValue)
{
    m_minX = store.GetDouble(MAP_MIN_X_KEY, defaultValue.minX());
    m_minY = store.GetDouble(MAP_MIN_Y_KEY, defaultValue.minY());
    m_maxX = store.GetDouble(MAP_MAX_X_KEY, defaultValue.maxX());
    m_maxY = store.GetDouble(MAP_MAX_Y_KEY, defaultValue.maxY());
    return true;
}

CPLJSONObject Envelope::save() const
{
    CPLJSONObject out;
    out.Add(MAP_MIN_X_KEY, m_minX);
    out.Add(MAP_MIN_Y_KEY, m_minY);
    out.Add(MAP_MAX_X_KEY, m_maxX);
    out.Add(MAP_MAX_Y_KEY, m_maxY);
    return out;
}

bool Envelope::intersects(const Envelope &other) const
{
    return m_minX <= other.m_maxX && m_maxX >= other.m_minX &&
            m_minY <= other.m_maxY && m_maxY >= other.m_minY;
}

bool Envelope::contains(const Envelope &other) const
{
    return m_minX <= other.m_minX && m_minY <= other.m_minY &&
            m_maxX >= other.m_maxX && m_maxY >= other.m_maxY;
}

const Envelope &Envelope::merge(const Envelope &other)
{
    m_minX = std::min(m_minX, other.m_minX);
    m_maxX = std::max(m_maxX, other.m_maxX);
    m_minY = std::min(m_minY, other.m_minY);
    m_maxY = std::max(m_maxY, other.m_maxY);

    return *this;
}

const Envelope &Envelope::intersect(const Envelope &other)
{
    if(intersects(other)) {
        if(isInit()) {
            m_minX = std::max(m_minX, other.m_minX);
            m_maxX = std::min(m_maxX, other.m_maxX);
            m_minY = std::max(m_minY, other.m_minY);
            m_maxY = std::min(m_maxY, other.m_maxY);
        }
        else {
            m_minX = other.m_minX;
            m_maxX = other.m_maxX;
            m_minY = other.m_minY;
            m_maxY = other.m_maxY;
        }
    }
    else {
        *this = Envelope();
    }
    return *this;
}

void Envelope::fix()
{
    if(m_minX > m_maxX) {
        std::swap(m_minX, m_maxX);
    }
    if(m_minY > m_maxY) {
        std::swap(m_minY, m_maxY);
    }
    if(isEqual(m_minX, m_maxX)) {
        m_minX -= std::numeric_limits<double>::epsilon();
        m_maxX += std::numeric_limits<double>::epsilon();
    }
    if(isEqual(m_minY, m_maxY)) {
        m_minY -= std::numeric_limits<double>::epsilon();
        m_maxY += std::numeric_limits<double>::epsilon();
    }
}

Normal ngsGetNormals(const SimplePoint &beg, const SimplePoint &end)
{
    float deltaX = static_cast<float>(end.x - beg.x);
    float deltaY = static_cast<float>(end.y - beg.y);

    float normX = -deltaY;
    float normY = deltaX;

    float norm_length = std::sqrt(normX * normX + normY * normY);

    if(norm_length == 0.0f)
        norm_length = 0.01f;

    //normalize the normal vector
    normX /= norm_length;
    normY /= norm_length;

    return {normX, normY};
}

OGRGeometry* ngsCreateGeometryFromGeoJson(const CPLJSONObject& json)
{
    return OGRGeometryFactory::createFromGeoJson(json);
}

bool geometryIntersects(const OGRGeometry& geometry, const Envelope env)
{
    return geometry.Intersects(env.toGeometry(nullptr).get());
}

inline long getPointId(
        const OGRPoint& pt, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(pt, env)) {
        return NOT_FOUND;
    }

    if(coordinates) {
        coordinates->setX(pt.getX());
        coordinates->setY(pt.getY());
    }
    return 0;
}

long getLineStringPointId(
        const OGRLineString& line, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(line, env)) {
        return NOT_FOUND;
    }

    long id = 0;
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
        return id;
    } else {
        return NOT_FOUND;
    }
}

long getPolygonPointId(
        const OGRPolygon& polygon, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(polygon, env)) {
        return NOT_FOUND;
    }

    const OGRLinearRing* ring = polygon.getExteriorRing();
    int numInteriorRings = polygon.getNumInteriorRings();
    long totalPointCount = 0;
    int k = 0;

    while(true) {
        if(!ring) {
            return NOT_FOUND;
        }

        long id = getLineStringPointId(*ring, env, coordinates);
        if(id >= 0) {
            return totalPointCount + id;
        }

        if(k >= numInteriorRings) {
            break;
        }

        totalPointCount += ring->getNumPoints();
        ring = polygon.getInteriorRing(k++);
    }

    return NOT_FOUND;
}

long getPolygonNumPoints(const OGRPolygon& polygon)
{
    const OGRLinearRing* ring = polygon.getExteriorRing();
    int numInteriorRings = polygon.getNumInteriorRings();
    long totalPointCount = 0;
    int k = 0;

    while(true) {
        if(!ring) {
            return totalPointCount;
        }

        totalPointCount += ring->getNumPoints();

        if(k >= numInteriorRings) {
            return totalPointCount;
        }

        ring = polygon.getInteriorRing(k++);
    }
}

long getMultiPointPointId(
        const OGRMultiPoint& mpt, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(mpt, env)) {
        return NOT_FOUND;
    }

    long id = 0;
    for(int k = 0, num = mpt.getNumGeometries(); k < num; ++k) {
        const OGRPoint* pt =
                static_cast<const OGRPoint*>(mpt.getGeometryRef(k));
        if(geometryIntersects(*pt, env)) {
            if(coordinates) {
                coordinates->setX(pt->getX());
                coordinates->setY(pt->getY());
            }
            return id;
        }
        ++id;
    }

    return NOT_FOUND;
}

long getMultiLineStringPointId(const OGRMultiLineString& mline,
        const Envelope env,
        OGRPoint* coordinates)
{
    if(!geometryIntersects(mline, env)) {
        return NOT_FOUND;
    }

    int numGeoms = mline.getNumGeometries();
    long totalPointCount = 0;
    int k = 0;

    while(true) {
        const OGRLineString* line =
                static_cast<const OGRLineString*>(mline.getGeometryRef(k++));
        long id = getLineStringPointId(*line, env, coordinates);
        if(id >= 0) {
            return totalPointCount + id;
        }

        if(k >= numGeoms) {
            break;
        }

        totalPointCount += line->getNumPoints();
    }

    return NOT_FOUND;
}

long getMultiPolygonPointId(const OGRMultiPolygon& mpolygon,
        const Envelope env,
        OGRPoint* coordinates)
{
    if(!geometryIntersects(mpolygon, env)) {
        return NOT_FOUND;
    }

    int numGeoms = mpolygon.getNumGeometries();
    long totalPointCount = 0;
    int k = 0;

    while(true) {
        const OGRPolygon* polygon =
                static_cast<const OGRPolygon*>(mpolygon.getGeometryRef(k++));
        long id = getPolygonPointId(*polygon, env, coordinates);
        if(id >= 0) {
            return totalPointCount + id;
        }

        if(k >= numGeoms) {
            break;
        }

        totalPointCount += getPolygonNumPoints(*polygon);
    }

    return NOT_FOUND;
}

long getGeometryPointId(
        const OGRGeometry& geometry, const Envelope env, OGRPoint* coordinates)
{
    long id = NOT_FOUND;
    switch(OGR_GT_Flatten(geometry.getGeometryType())) {
        case wkbPoint: {
            const OGRPoint& pt = static_cast<const OGRPoint&>(geometry);
            id = getPointId(pt, env, coordinates);
            break;
        }
        case wkbLineString: {
            const OGRLineString& lineString =
                    static_cast<const OGRLineString&>(geometry);
            id = getLineStringPointId(lineString, env, coordinates);
            break;
        }
        case wkbPolygon: {
            const OGRPolygon& polygon =
                    static_cast<const OGRPolygon&>(geometry);
            id = getPolygonPointId(polygon, env, coordinates);
            break;
        }
        case wkbMultiPoint: {
            const OGRMultiPoint& mpt =
                    static_cast<const OGRMultiPoint&>(geometry);
            id = getMultiPointPointId(mpt, env, coordinates);
            break;
        }
        case wkbMultiLineString: {
            const OGRMultiLineString& mline =
                    static_cast<const OGRMultiLineString&>(geometry);
            id = getMultiLineStringPointId(mline, env, coordinates);
            break;
        }
        case wkbMultiPolygon: {
            const OGRMultiPolygon& mpolygon =
                    static_cast<const OGRMultiPolygon&>(geometry);
            id = getMultiPolygonPointId(mpolygon, env, coordinates);
            break;
        }
    }
    return id;
}

bool shiftPoint(
        OGRPoint& pt, long id, const OGRRawPoint& offset, OGRPoint* coordinates)
{
    if(0 != id) {
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
        long id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    if(id < 0 && id >= lineString.getNumPoints()) {
        return false;
    }

    OGRPoint pt;
    lineString.getPoint(id, &pt);
    lineString.setPoint(id, pt.getX() + offset.x, pt.getY() + offset.y);
    if(coordinates) {
        coordinates->setX(pt.getX());
        coordinates->setY(pt.getY());
    }
    return true;
}

bool shiftPolygonPoint(OGRPolygon& polygon,
        long id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    if(id < 0) {
        return false;
    }

    OGRLinearRing* ring = polygon.getExteriorRing();
    int numInteriorRings = polygon.getNumInteriorRings();
    long totalPointCount = 0;
    int i = 0;

    while(true) {
        if(!ring) {
            return false;
        }

        long ringPtId = id - totalPointCount;
        int ringNumPoints = ring->getNumPoints();
        if(ringPtId < ringNumPoints) {
            return shiftLineStringPoint(*ring, ringPtId, offset, coordinates);
        }

        if(i >= numInteriorRings) {
            return false;
        }

        totalPointCount += ringNumPoints;
        ring = polygon.getInteriorRing(i++);
    }
}

bool shiftMultiPointPoint(OGRMultiPoint& mpt,
        long id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    if(id < 0 || id >= mpt.getNumGeometries()) {
        return false;
    }
    OGRPoint* pt = static_cast<OGRPoint*>(mpt.getGeometryRef(id));
    return shiftPoint(*pt, 0, offset, coordinates);
}

bool shiftMultiLineStringPoint(OGRMultiLineString& mline,
        long id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    if(id < 0) {
        return false;
    }

    long totalPointCount = 0;
    for(int k = 0, num = mline.getNumGeometries(); k < num; ++k) {
        OGRLineString* line =
                static_cast<OGRLineString*>(mline.getGeometryRef(k));
        int lineNumPoints = line->getNumPoints();
        long linePtId = id - totalPointCount;

        if(linePtId < lineNumPoints) {
            return shiftLineStringPoint(*line, linePtId, offset, coordinates);
        }
        totalPointCount += lineNumPoints;
    }
    return false;
}

bool shiftMultiPolygonPoint(OGRMultiPolygon& mpolygon,
        long id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    if(id < 0) {
        return false;
    }

    long totalPointCount = 0;
    for(int k = 0, num = mpolygon.getNumGeometries(); k < num; ++k) {
        OGRPolygon* polygon =
                static_cast<OGRPolygon*>(mpolygon.getGeometryRef(k));
        int polyNumPoints = getPolygonNumPoints(*polygon);
        long polyPtId = id - totalPointCount;

        if(polyPtId < polyNumPoints) {
            return shiftPolygonPoint(*polygon, polyPtId, offset, coordinates);
        }
        totalPointCount += polyNumPoints;
    }
    return false;
}

bool shiftGeometryPoint(OGRGeometry& geometry,
        long id,
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
