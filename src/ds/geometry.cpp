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

inline bool geometryIntersects(const OGRGeometry& geometry, Envelope env)
{
    return geometry.Intersects(env.toGeometry(nullptr).get());
}

inline long getPointId(const OGRPoint& pt, Envelope env)
{
    if (!geometryIntersects(pt, env)) {
        return NOT_FOUND;
    }
    return 0;
}

long getLineStringPointId(const OGRLineString& lineString, Envelope env)
{
    if (!geometryIntersects(lineString, env)) {
        return NOT_FOUND;
    }

    long id = NOT_FOUND;
    OGRPointIterator* it = lineString.getPointIterator();
    OGRPoint pt;
    while (it->getNextPoint(&pt)) {
        if (geometryIntersects(pt, env)) {
            ++id;
            break;
        }
        ++id;
    }
    OGRPointIterator::destroy(it);
    return id;
}

long getPolygonPointId(const OGRPolygon& polygon, Envelope env)
{
    if (!geometryIntersects(polygon, env)) {
        return NOT_FOUND;
    }

    const OGRLinearRing* ring = polygon.getExteriorRing();
    int numInteriorRings = polygon.getNumInteriorRings();
    long totalPointCount = 0;
    int i = 0;

    while (true) {
        if (!ring) {
            return NOT_FOUND;
        }

        long id = getLineStringPointId(*ring, env);
        if (id >= 0) {
            return totalPointCount + id;
        }

        if (i >= numInteriorRings) {
            break;
        }

        totalPointCount += ring->getNumPoints();
        ring = polygon.getInteriorRing(i++);
    }

    return NOT_FOUND;
}

long getGeometryPointId(const OGRGeometry& geometry, Envelope env)
{
    long id = NOT_FOUND;
    switch(OGR_GT_Flatten(geometry.getGeometryType())) {
        case wkbPoint: {
            const OGRPoint& pt =
                    static_cast<const OGRPoint&>(geometry);
            id = getPointId(pt, env);
            break;
        }
        case wkbLineString: {
            const OGRLineString& lineString =
                    static_cast<const OGRLineString&>(geometry);
            id = getLineStringPointId(lineString, env);
            break;
        }
        case wkbPolygon: {
            const OGRPolygon& polygon =
                    static_cast<const OGRPolygon&>(geometry);
            id = getPolygonPointId(polygon, env);
            break;
        }
        case wkbMultiPoint: {
            break;
        }
        case wkbMultiLineString: {
            break;
        }
        case wkbMultiPolygon: {
            break;
        }
    }
    return id;
}

bool shiftPoint(
        OGRPoint& pt, long id, const OGRRawPoint& offset)
{
    if (0 != id) {
        return false;
    }

    pt.setX(pt.getX() + offset.x);
    pt.setY(pt.getY() + offset.y);
    return true;
}

bool shiftLineStringPoint(
        OGRLineString& lineString, long id, const OGRRawPoint& offset)
{
    if (id < 0 && id > lineString.getNumPoints()) {
        return false;
    }

    OGRPoint pt;
    lineString.getPoint(id, &pt);
    lineString.setPoint(id, pt.getX() + offset.x, pt.getY() + offset.y);
    return true;
}

bool shiftPolygonPoint(
        OGRPolygon& polygon, long id, const OGRRawPoint& offset)
{
    if (id < 0) {
        return false;
    }

    OGRLinearRing* ring = polygon.getExteriorRing();
    int numInteriorRings = polygon.getNumInteriorRings();
    long totalPointCount = 0;
    int i = 0;

    while (true) {
        if (!ring) {
            return false;
        }

        long ringPtId = id - totalPointCount;
        int ringNumPoints = ring->getNumPoints();
        if (ringPtId < ringNumPoints) {
            return shiftLineStringPoint(*ring, ringPtId, offset);
        }

        if (i >= numInteriorRings) {
            break;
        }

        totalPointCount += ringNumPoints;
        ring = polygon.getInteriorRing(i++);
    }

    return true;
}

bool shiftGeometryPoint(
        OGRGeometry& geometry, long id, const OGRRawPoint& offset)
{
    switch(OGR_GT_Flatten(geometry.getGeometryType())) {
        case wkbPoint: {
            OGRPoint& pt = static_cast<OGRPoint&>(geometry);
            return shiftPoint(pt, id, offset);
        }
        case wkbLineString: {
            OGRLineString& lineString =
                    static_cast<OGRLineString&>(geometry);
            return shiftLineStringPoint(lineString, id, offset);
        }
        case wkbPolygon: {
            OGRPolygon& polygon =
                    static_cast<OGRPolygon&>(geometry);
            return shiftPolygonPoint(polygon, id, offset);
        }
        case wkbMultiPoint: {
            return false;
        }
        case wkbMultiLineString: {
            return false;
        }
        case wkbMultiPolygon: {
            return false;
        }
        default: {
            return false;
        }
    }
}

} // namespace ngs
