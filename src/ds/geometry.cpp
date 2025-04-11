/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2025 NextGIS, <info@nextgis.com>
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
#include <algorithm>

#include "geos_c.h"

#include "api_priv.h"

namespace ngs {

constexpr double DBLNAN = 0.0;

constexpr const char *MAP_MIN_X_KEY = "min_x";
constexpr const char *MAP_MIN_Y_KEY = "min_y";
constexpr const char *MAP_MAX_X_KEY = "max_x";
constexpr const char *MAP_MAX_Y_KEY = "max_y";

//------------------------------------------------------------------------------
// GeometryPtr
//------------------------------------------------------------------------------

GeometryPtr::GeometryPtr(OGRGeometry *geom) :
    shared_ptr(geom, OGRGeometryFactory::destroyGeometry)
{
}

GeometryPtr::GeometryPtr() :
    shared_ptr(nullptr, OGRGeometryFactory::destroyGeometry)
{
}

GeometryPtr &GeometryPtr::operator=(OGRGeometry *geom)
{
    reset(geom);
    return *this;
}

ngs::GeometryPtr::operator OGRGeometry *() const
{
     return get();
}

//------------------------------------------------------------------------------
// Envelope
//------------------------------------------------------------------------------

Envelope::Envelope(const OGREnvelope &env) :
    m_minX(env.MinX),
    m_minY(env.MinY),
    m_maxX(env.MaxX),
    m_maxY(env.MaxY)
{

}

void Envelope::set(const OGREnvelope &env)
{
    m_minX = env.MinX;
    m_minY = env.MinY;
    m_maxX = env.MaxX;
    m_maxY = env.MaxY;
}

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

GeometryPtr Envelope::toGeometry(SpatialReferencePtr spatialRef) const
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
    rgn->assignSpatialReference(spatialRef);
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

bool Envelope::load(const CPLJSONObject &store, const Envelope &defaultValue)
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
    if(isInit()) {
        m_minX = std::min(m_minX, other.m_minX);
        m_maxX = std::max(m_maxX, other.m_maxX);
        m_minY = std::min(m_minY, other.m_minY);
        m_maxY = std::max(m_maxY, other.m_maxY);
    }
    else {
        m_minX = other.m_minX;
        m_maxX = other.m_maxX;
        m_minY = other.m_minY;
        m_maxY = other.m_maxY;
    }
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


//------------------------------------------------------------------------------
// GEOSGeometryWrap
//------------------------------------------------------------------------------
GEOSGeometryWrap::GEOSGeometryWrap(GEOSGeom geom, GEOSContextHandlePtr handle) :
    m_geom(geom),
    m_geosHandle(handle)
{
}

GEOSGeometryWrap::GEOSGeometryWrap(OGRGeometry *geom) : m_geom(nullptr)
{
    if(nullptr != geom) {
        m_geom = geom->exportToGEOS(m_geosHandle.get());
    }
}

GEOSGeometryWrap::~GEOSGeometryWrap()
{
    GEOSGeom_destroy_r(m_geosHandle.get(), m_geom);
}

int GEOSGeometryWrap::type() const
{
    return GEOSGeomTypeId_r(m_geosHandle.get(), m_geom);
}

GEOSGeometryPtr GEOSGeometryWrap::clip(const Envelope& env) const
{
    GEOSGeom clipped = GEOSClipByRect_r(m_geosHandle.get(), m_geom,
                                        env.minX(), env.minY(),
                                        env.maxX(), env.maxY());
    return GEOSGeometryPtr(new GEOSGeometryWrap(clipped, m_geosHandle));
}

static OGRRawPoint generalize(double x, double y, double step)
{
    OGRRawPoint out;
    out.x = static_cast<long>(x / step) * step;
    out.y = static_cast<long>(y / step) * step;
    return out;
}

GEOSGeom GEOSGeometryWrap::generalizePoint(const GEOSGeom_t *geom, double step)
{
    double x, y;
    const GEOSCoordSequence* cs = GEOSGeom_getCoordSeq_r(m_geosHandle.get(), geom);

    GEOSCoordSeq_getX_r(m_geosHandle.get(), cs, 0, &x);
    GEOSCoordSeq_getY_r(m_geosHandle.get(), cs, 0, &y);
    OGRRawPoint gpoint = generalize(x, y, step);

    GEOSCoordSeq ncs = GEOSCoordSeq_clone_r(m_geosHandle.get(), cs);

    GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 0, gpoint.x);
    GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 0, gpoint.y);

    return GEOSGeom_createPoint_r(m_geosHandle.get(), ncs);
}

GEOSGeom GEOSGeometryWrap::generalizeMultiPoint(const GEOSGeom_t *geom,
                                                double step)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        CPLError(CE_Failure, CPLE_ObjectNull, "Geometry has no parts");
        return nullptr;
    }
    std::vector<GEOSGeom> parts;
    OGRRawPoint prevGpoint(BIG_VALUE, BIG_VALUE);
    double x, y;

    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t *g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);
        const GEOSCoordSequence* cs = GEOSGeom_getCoordSeq_r(m_geosHandle.get(),
                                                             g);

        GEOSCoordSeq_getX_r(m_geosHandle.get(), cs, 0, &x);
        GEOSCoordSeq_getY_r(m_geosHandle.get(), cs, 0, &y);
        OGRRawPoint gpoint = generalize(x, y, step);

        if(isEqual(prevGpoint.x, gpoint.x) && isEqual(prevGpoint.y, gpoint.y)) {
            continue;
        }

        GEOSCoordSequence *ncs = GEOSCoordSeq_clone_r(m_geosHandle.get(), cs);

        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 0, gpoint.x);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 0, gpoint.y);

        parts.push_back(GEOSGeom_createPoint_r(m_geosHandle.get(), ncs));
        prevGpoint = gpoint;
    }

    if(parts.empty()) {
        return nullptr;
    }

    return GEOSGeom_createCollection_r(m_geosHandle.get(), GEOS_MULTIPOINT,
                                        parts.data(), parts.size());

}

GEOSGeom GEOSGeometryWrap::generalizeLine(const GEOSGeom_t *geom, double step,
                                          bool isRing)
{
    const GEOSCoordSequence *cs = GEOSGeom_getCoordSeq_r(m_geosHandle.get(),
                                                         geom);
    unsigned int count = 0;
    GEOSCoordSeq_getSize_r(m_geosHandle.get(), cs, &count);

    OGRRawPoint prevGpoint(BIG_VALUE, BIG_VALUE);
    double x, y;
    std::vector<OGRRawPoint> parts;
    struct OGRRawPointIs {
        OGRRawPointIs( OGRRawPoint s ) : toFind(s) { }
        bool operator() (const OGRRawPoint &n) {
            return isEqual(n.x, toFind.x) && isEqual(n.y, toFind.y);
        }
        OGRRawPoint toFind;
    };

    for(unsigned int i = 0; i < count; ++i) {
        GEOSCoordSeq_getX_r(m_geosHandle.get(), cs, i, &x);
        GEOSCoordSeq_getY_r(m_geosHandle.get(), cs, i, &y);
        OGRRawPoint gpoint = generalize(x, y, step);

        if(isRing && std::find_if(parts.begin(), parts.end(),
                                  OGRRawPointIs(gpoint)) != parts.end()) {
            continue;
        }

        if(isEqual(prevGpoint.x, gpoint.x) && isEqual(prevGpoint.y, gpoint.y)) {
            continue;
        }

        parts.push_back(gpoint);
        prevGpoint = gpoint;
    }

    if(parts.size() < 2) {
        return nullptr;
    }

    if(isRing) {
        if(parts.size() < 3) {
            return nullptr;
        }
        parts.push_back(parts.front());
    }

    GEOSCoordSeq ncs = GEOSCoordSeq_create_r(m_geosHandle.get(),
                                             static_cast<unsigned int>(parts.size()),
                                             2);
    unsigned int counter = 0;
    CPLDebug("ngstore", "parts - %ld", parts.size());
    for(const OGRRawPoint &pt : parts) {
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, counter, pt.x);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, counter, pt.y);
        counter++;
    }

    if(isRing) {
        return GEOSGeom_createLinearRing_r(m_geosHandle.get(), ncs);
    }

    return GEOSGeom_createLineString_r(m_geosHandle.get(), ncs);
}

GEOSGeom GEOSGeometryWrap::generalizeMultiLine(const GEOSGeom_t *geom,
                                               double step)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        CPLError(CE_Failure, CPLE_ObjectNull, "Geometry has no parts");
        return nullptr;
    }
    std::vector<GEOSGeom> parts;
    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t* g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);
        GEOSGeom ng = generalizeLine(g, step);
        if(nullptr != ng) {
            parts.push_back(ng);
        }
    }

    if(parts.empty()) {
        return nullptr;
    }

    return  GEOSGeom_createCollection_r(m_geosHandle.get(), GEOS_MULTILINESTRING,
                                        parts.data(), parts.size());
}

GEOSGeom GEOSGeometryWrap::generalizePolygon(const GEOSGeom_t *geom, double step)
{
    GEOSGeom env = GEOSEnvelope_r(m_geosHandle.get(), geom);
    if(nullptr == env || GEOSisEmpty_r(m_geosHandle.get(), env) == 1) {
        CPLDebug("ngstore", "Empty or wrong polygon to generalize");
        return nullptr;
    }

    const GEOSGeometry* exteriorRing = GEOSGetExteriorRing_r(m_geosHandle.get(),
                                                                 env);

    const GEOSCoordSequence* cs = GEOSGeom_getCoordSeq_r(m_geosHandle.get(),
                                                         exteriorRing);
    if(nullptr == cs) {
        GEOSGeom_destroy_r(m_geosHandle.get(), env);
        return nullptr;
    }

    double x(0.0), y(0.0);
    GEOSCoordSeq_getX_r(m_geosHandle.get(), cs, 0, &x);
    GEOSCoordSeq_getY_r(m_geosHandle.get(), cs, 0, &y);
    Envelope extent;
    extent.setMinX(x);
    extent.setMinY(y);
    GEOSCoordSeq_getX_r(m_geosHandle.get(), cs, 2, &x);
    GEOSCoordSeq_getY_r(m_geosHandle.get(), cs, 2, &y);
    extent.setMaxX(x);
    extent.setMaxY(y);
    extent.fix();
    if(extent.width() < step || extent.height() < step) {
        CPLDebug("ngstore", "Too small generalize polygon for step %f", step);
        return env;
    }

    GEOSGeom simple = GEOSSimplify_r(m_geosHandle.get(), geom, step * .25);
    if(nullptr == simple || GEOSisEmpty_r(m_geosHandle.get(), simple) == 1) {
        CPLDebug("ngstore", "Simplify generalize polygon failed");
        return env;
    }

    GEOSGeom_destroy_r(m_geosHandle.get(), env);
    return simple;


    /*const GEOSGeometry* exteriorRing = GEOSGetExteriorRing_r(m_geosHandle.get(),
                                                             geom);

    GEOSGeom newRing = generalizeLine(exteriorRing, step, true);
    if(nullptr == newRing || GEOSisEmpty_r(m_geosHandle.get(), newRing) == 1) {
        return nullptr;
    }

    int count = GEOSGetNumInteriorRings_r(m_geosHandle.get(), geom);
    unsigned int counter = 0;
    GEOSGeom interiorRings[count];
    for(int i = 0; i < count; ++i) {
        const GEOSGeometry* interiorRing = GEOSGetInteriorRingN_r(
                    m_geosHandle.get(), geom, i);
        GEOSGeom newInteriorRing = generalizeLine(interiorRing, step, true);
        if(nullptr == newInteriorRing || GEOSisEmpty_r(m_geosHandle.get(), newRing) == 1) {
            continue;
        }
        interiorRings[counter++] = newInteriorRing;
    }

    GEOSGeom p = GEOSGeom_createPolygon_r(m_geosHandle.get(), newRing, interiorRings,
                                     counter);
    if(GEOSisValid_r(m_geosHandle.get(), p) == 1) {
        return p;
    }

    GEOSGeom_destroy_r(m_geosHandle.get(), p);
    return nullptr;
    */
}

GEOSGeom GEOSGeometryWrap::generalizeMultiPolygon(const GEOSGeom_t *geom,
                                                  double step)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        CPLError(CE_Failure, CPLE_ObjectNull, "Geometry has no parts");
        return nullptr;
    }
    std::vector<GEOSGeom> parts;
    unsigned int counter = 0;
    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t *g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);
        GEOSGeom ng = generalizePolygon(g, step);
        if(nullptr != ng) {
            parts.push_back(ng);
        }
    }

    if(parts.empty()) {
        return nullptr;
    }

    return  GEOSGeom_createCollection_r(m_geosHandle.get(), GEOS_MULTIPOLYGON,
                                        parts.data(), parts.size());
}

void GEOSGeometryWrap::setCentroid(int type)
{
    GEOSGeom g = GEOSGetCentroid_r(m_geosHandle.get(), m_geom);
    GEOSGeom_destroy_r(m_geosHandle.get(), m_geom);
    if(type == GEOS_POINT) {
        m_geom = g;
    }
    else if(type == GEOS_LINESTRING) {
        double x,y;
        GEOSGeomGetX_r(m_geosHandle.get(), g, &x);
        GEOSGeomGetY_r(m_geosHandle.get(), g, &y);
        GEOSGeom_destroy_r(m_geosHandle.get(), g);

        GEOSCoordSeq ncs = GEOSCoordSeq_create_r(m_geosHandle.get(), 2, 2);
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 0, x - 1.5);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 0, y - 1.5);
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 1, x + 1.5);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 1, y + 1.5);

        m_geom = GEOSGeom_createLineString_r(m_geosHandle.get(), ncs);
    }
    else if(type == GEOS_POLYGON) {
        double x,y;
        GEOSGeomGetX_r(m_geosHandle.get(), g, &x);
        GEOSGeomGetY_r(m_geosHandle.get(), g, &y);
        GEOSGeom_destroy_r(m_geosHandle.get(), g);

        GEOSCoordSeq ncs = GEOSCoordSeq_create_r(m_geosHandle.get(), 4, 2);
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 0, x - 1.5);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 0, y - 1.5);
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 1, x - 1.5);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 1, y + 1.5);
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 2, x + 1.5);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 2, y + 1.5);
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 3, x - 1.5);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 3, y - 1.5);

        GEOSGeom ring = GEOSGeom_createLinearRing_r(m_geosHandle.get(), ncs);
        m_geom = GEOSGeom_createPolygon_r(m_geosHandle.get(), ring, nullptr, 0);
    }
}

void GEOSGeometryWrap::simplify(double step)
{
    if(isEqual(step, 0.0) || nullptr == m_geom) {
        return;
    }

    GEOSGeom g;
    switch(type()) {
    case GEOS_POINT:
        g = generalizePoint(m_geom, step);
        GEOSGeom_destroy_r(m_geosHandle.get(), m_geom);
        m_geom = g;
        break;
    case GEOS_LINESTRING:
        g = generalizeLine(m_geom, step);
        if(nullptr == g) {
            setCentroid(GEOS_LINESTRING);
        }
        else {
            GEOSGeom_destroy_r(m_geosHandle.get(), m_geom);
            m_geom = g;
        }
        break;
    case GEOS_POLYGON:
        g = generalizePolygon(m_geom, step);
        if(nullptr == g) {
            setCentroid(GEOS_POLYGON);
        }
        else {
            GEOSGeom_destroy_r(m_geosHandle.get(), m_geom);
            m_geom = g;
        }
        break;
    case GEOS_MULTIPOINT:
        g = generalizeMultiPoint(m_geom, step);
        if(nullptr == g) {
            setCentroid(GEOS_POINT);
        }
        else {
            GEOSGeom_destroy_r(m_geosHandle.get(), m_geom);
            m_geom = g;
        }
        break;
    case GEOS_MULTILINESTRING:
        g = generalizeMultiLine(m_geom, step);
        if(nullptr == g) {
            setCentroid(GEOS_LINESTRING);
        }
        else {
            GEOSGeom_destroy_r(m_geosHandle.get(), m_geom);
            m_geom = g;
        }
        break;
    case GEOS_MULTIPOLYGON:
        g = generalizeMultiPolygon(m_geom, step);
        if(nullptr == g) {
            setCentroid(GEOS_POLYGON);
        }
        else {
            GEOSGeom_destroy_r(m_geosHandle.get(), m_geom);
            m_geom = g;
        }
        break;
    case GEOS_LINEARRING:
    case GEOS_GEOMETRYCOLLECTION:
    default:
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Expectred point/line/polygon/multipoint/multiline/multipolygon here");
        break;
    }
}

double GEOSGeometryWrap::distance(double x, double y) const
{
    GEOSCoordSequence *seq = GEOSCoordSeq_create_r(m_geosHandle.get(), 1, 2);
    GEOSCoordSeq_setX_r(m_geosHandle.get(), seq, 0, x);
    GEOSCoordSeq_setY_r(m_geosHandle.get(), seq, 0, y);
    GEOSGeom geomPt = GEOSGeom_createPoint_r(m_geosHandle.get(), seq);
    double dist(22000000.0);
    GEOSDistance_r(m_geosHandle.get(), m_geom, geomPt, &dist);
    GEOSGeom_destroy_r(m_geosHandle.get(), geomPt);
    return dist;
}

bool GEOSGeometryWrap::intersects(double x, double y) const
{
    GEOSCoordSequence *seq = GEOSCoordSeq_create_r(m_geosHandle.get(), 1, 2);
    GEOSCoordSeq_setX_r(m_geosHandle.get(), seq, 0, x);
    GEOSCoordSeq_setY_r(m_geosHandle.get(), seq, 0, y);
    GEOSGeom geomPt = GEOSGeom_createPoint_r(m_geosHandle.get(), seq);
    bool result = GEOSIntersects_r(m_geosHandle.get(), m_geom, geomPt) == 1;
    GEOSGeom_destroy_r(m_geosHandle.get(), geomPt);
    return result;
}

//------------------------------------------------------------------------------

/**
 * @brief ngsGetMedianPoint computes the median point
 * between given points pt1 and pt2.
 *
 * @param pt1
 * @param pt2
 * @return Median point between given points pt1 and pt2.
 */
//SimplePoint ngsGetMedianPoint(const SimplePoint &pt1, const SimplePoint &pt2)
//{
//    return {(pt2.x - pt1.x) / 2 + pt1.x, (pt2.y - pt1.y) / 2 + pt1.y};
//}

OGRGeometry *ngsCreateGeometryFromGeoJson(const CPLJSONObject &json)
{
    return OGRGeometryFactory::createFromGeoJson(json);
}

bool ngsIsGeometryIntersectsEnvelope(const OGRGeometry &geometry,
                                     const Envelope &env)
{
    OGREnvelope ogrEnv;
    geometry.getEnvelope(&ogrEnv);
    return ogrEnv.Intersects(env.toOgrEnvelope());
}


} // namespace ngs
