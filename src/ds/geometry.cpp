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

#include "geos_c.h"

#include "api_priv.h"

namespace ngs {

constexpr double DBLNAN = 0.0;

constexpr const char* MAP_MIN_X_KEY = "min_x";
constexpr const char* MAP_MIN_Y_KEY = "min_y";
constexpr const char* MAP_MAX_X_KEY = "max_x";
constexpr const char* MAP_MAX_Y_KEY = "max_y";

//------------------------------------------------------------------------------
// VectorTileItem
//------------------------------------------------------------------------------

VectorTileItem::VectorTileItem() :
     m_valid(false),
     m_2d(true)
{

}

void VectorTileItem::removeId(GIntBig id)
{
    auto it = m_ids.find(id);
    if(it != m_ids.end()) {
        m_ids.erase(it);
        if(m_ids.empty()) {
            m_valid = false;
        }
    }
}

void VectorTileItem::addBorderIndex(unsigned short ring, unsigned short index)
{
    if(m_borderIndices.size() <= ring) {
       for(unsigned short i = static_cast<unsigned short>(m_borderIndices.size());
           i < ring + 1; ++i) {
            m_borderIndices.push_back(std::vector<unsigned short>());
        }
    }
    m_borderIndices[ring].push_back(index);
}

void VectorTileItem::save(Buffer* buffer)
{
    buffer->put(static_cast<GByte>(m_2d));

    // vector<SimplePoint> m_points
    buffer->put(static_cast<GUInt32>(m_points.size()));
    for(auto point : m_points) {
        if(m_2d) {
            buffer->put(point.x);
            buffer->put(point.y);
        }
        else {
            // TODO: Add point with z support
        }
    }

    // vector<unsigned short> m_indices
    buffer->put(static_cast<GUInt32>(m_indices.size()));
    for(auto index : m_indices) {
        buffer->put(index);
    }

    //vector<vector<unsigned short>> m_borderIndices
    buffer->put(static_cast<GUInt32>(m_borderIndices.size()));
    for(auto borderIndexArray : m_borderIndices) {
        buffer->put(static_cast<GUInt32>(borderIndexArray.size()));
        for(auto borderIndex : borderIndexArray) {
            buffer->put(borderIndex);
        }
    }

    // vector<SimplePoint> m_centroids
    buffer->put(static_cast<GUInt32>(m_centroids.size()));
    for(auto centroid : m_centroids) {
        if(m_2d) {
            buffer->put(centroid.x);
            buffer->put(centroid.y);
        }
        else {
            // TODO: Add point with z support
        }
    }

    // set<GIntBig> m_ids
    buffer->put(static_cast<GUInt32>(m_ids.size()));
    for(auto id : m_ids) {
        buffer->put(id);
    }
}

bool VectorTileItem::load(Buffer& buffer)
{
    m_2d = buffer.getByte();
    // vector<SimplePoint> m_points
    GUInt32 size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        if(m_2d) {
            float x = buffer.getFloat();
            float y = buffer.getFloat();
            SimplePoint pt = {x, y};
            m_points.push_back(pt);
        }
        else {
            // TODO: Add point with z support
        }
    }

    // vector<unsigned short> m_indices
    size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        m_indices.push_back(buffer.getUShort());
    }

    //vector<vector<unsigned short>> m_borderIndices
    size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        GUInt32 size1 = buffer.getULong();
        std::vector<unsigned short> array;
        for(GUInt32 j = 0; j < size1; ++j) {
            array.push_back(buffer.getUShort());
        }
        if(!array.empty()) {
            m_borderIndices.push_back(array);
        }
    }

    // vector<SimplePoint> m_centroids
    size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        if(m_2d) {
            float x = buffer.getFloat();
            float y = buffer.getFloat();
            SimplePoint pt = {x, y};
            m_centroids.push_back(pt);
        }
        else {
            // TODO: Add point with z support
        }
    }

    // set<GIntBig> m_ids
    size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        m_ids.insert(buffer.getBig());
    }

    m_valid = true;
    return true;
}

bool VectorTileItem::isClosed() const
{
    return isEqual(m_points.front().x, m_points.back().x) &&
            isEqual(m_points.front().y, m_points.back().y);
}

void VectorTileItem::loadIds(const VectorTileItem &item)
{
    for(GIntBig id : item.m_ids) {
        m_ids.insert(id);
    }
}

bool VectorTileItem::isIdsPresent(const std::set<GIntBig> &other, bool full) const
{
    if(other.empty()) {
        return false;
    }
    if(full) {
        return  std::includes(other.begin(), other.end(),
                             m_ids.begin(), m_ids.end());
    }
    else {
        for(auto id : other) {
            if(m_ids.find(id) != m_ids.end()) {
                return true;
            }
        }
//        result = std::search(other.begin(), other.end(),
//                           m_ids.begin(), m_ids.end()) != other.end();
    }

    return false;
}
std::set<GIntBig> VectorTileItem::idsIntesect(const std::set<GIntBig> &other) const
{
    std::set<GIntBig> common_data;
    if(other.empty())
        return common_data;
    std::set_intersection(other.begin(), other.end(),
                          m_ids.begin(), m_ids.end(),
                          std::inserter(common_data, common_data.begin()));
    return common_data;
}


//------------------------------------------------------------------------------
// VectorTile
//------------------------------------------------------------------------------

void VectorTile::add(const VectorTileItem &item, bool checkDuplicates)
{
    if(!item.isValid()) {
        return;
    }
    if(checkDuplicates) {
        auto it = std::find(m_items.begin(), m_items.end(), item);
        if(it == m_items.end()) {
            m_items.push_back(item);
        }
        else {
            (*it).loadIds(item);
        }
    }
    else {
        m_items.push_back(item);
    }

    if(!m_valid) {
        m_valid = !m_items.empty();
    }
}

void VectorTile::add(const VectorTileItemArray& items,
                     bool checkDuplicates)
{
    for(auto item : items) {
        add(item, checkDuplicates);
    }
}

void VectorTile::remove(GIntBig id)
{
    auto it = m_items.begin();
    while(it != m_items.end()) {
        (*it).removeId(id);
        if((*it).isValid() == false) {
            it = m_items.erase(it);
        }
        else {
            ++it;
        }
    }
}

BufferPtr VectorTile::save()
{
    BufferPtr buff(new Buffer);
    buff->put(static_cast<GUInt32>(m_items.size()));
    for(auto item : m_items) {
        item.save(buff.get());
    }
    return buff;
}

bool VectorTile::load(Buffer& buffer)
{
    GUInt32 size = buffer.getULong();
    for(GUInt32 i = 0; i < size; ++i) {
        VectorTileItem item;
        item.load(buffer);
        m_items.push_back(item);
    }
    m_valid = true;
    return true;
}

bool VectorTile::empty() const
{
    if(!m_items.empty()) {
        for(auto item : m_items) {
            if(item.pointCount() > 0) {
                return false;
            }
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// Envelope
//------------------------------------------------------------------------------

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

Normal ngsGetNormals(const SimplePoint &beg, const SimplePoint &end)
{
    float deltaX = static_cast<float>(end.x - beg.x);
    float deltaY = static_cast<float>(end.y - beg.y);

    float normX = -deltaY;
    float normY = deltaX;

    float norm_length = std::sqrt(normX * normX + normY * normY);

    if(isEqual(norm_length, 0.0f))
        norm_length = 0.01f;

    //normalize the normal vector
    normX /= norm_length;
    normY /= norm_length;

    return {normX, normY};
}

//------------------------------------------------------------------------------
// GEOSGeometryWrap
//------------------------------------------------------------------------------
GEOSGeometryWrap::GEOSGeometryWrap(GEOSGeom geom, GEOSContextHandlePtr handle) :
    m_geom(geom),
    m_geosHandle(handle)
{
}

GEOSGeometryWrap::GEOSGeometryWrap(OGRGeometry* geom) : m_geom(nullptr)
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

GEOSGeom GEOSGeometryWrap::generalizePoint(const GEOSGeom_t* geom, double step)
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

GEOSGeom GEOSGeometryWrap::generalizeMultiPoint(const GEOSGeom_t* geom,
                                                double step)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        CPLError(CE_Failure, CPLE_ObjectNull, "Geometry has no parts");
        return nullptr;
    }
    GEOSGeom parts[count];
    unsigned int counter = 0;
    OGRRawPoint prevGpoint(BIG_VALUE, BIG_VALUE);
    double x, y;

    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t* g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);
        const GEOSCoordSequence* cs = GEOSGeom_getCoordSeq_r(m_geosHandle.get(),
                                                             g);

        GEOSCoordSeq_getX_r(m_geosHandle.get(), cs, 0, &x);
        GEOSCoordSeq_getY_r(m_geosHandle.get(), cs, 0, &y);
        OGRRawPoint gpoint = generalize(x, y, step);

        if(isEqual(prevGpoint.x, gpoint.x) && isEqual(prevGpoint.y, gpoint.y)) {
            continue;
        }

        GEOSCoordSequence* ncs = GEOSCoordSeq_clone_r(m_geosHandle.get(), cs);

        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 0, gpoint.x);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 0, gpoint.y);

        parts[counter++] = GEOSGeom_createPoint_r(m_geosHandle.get(), ncs);
        prevGpoint = gpoint;
    }

    if(counter == 0) {
        return nullptr;
    }

    return  GEOSGeom_createCollection_r(m_geosHandle.get(), GEOS_MULTIPOINT,
                                        parts, counter);

}

GEOSGeom GEOSGeometryWrap::generalizeLine(const GEOSGeom_t* geom, double step)
{
    const GEOSCoordSequence* cs = GEOSGeom_getCoordSeq_r(m_geosHandle.get(),
                                                         geom);
    unsigned int count = 0;
    GEOSCoordSeq_getSize_r(m_geosHandle.get(), cs, &count);

    OGRRawPoint prevGpoint(BIG_VALUE, BIG_VALUE);
    double x, y;
    std::vector<OGRRawPoint> parts;
    for(unsigned int i = 0; i < count; ++i) {
        GEOSCoordSeq_getX_r(m_geosHandle.get(), cs, i, &x);
        GEOSCoordSeq_getY_r(m_geosHandle.get(), cs, i, &y);
        OGRRawPoint gpoint = generalize(x, y, step);

        if(isEqual(prevGpoint.x, gpoint.x) && isEqual(prevGpoint.y, gpoint.y)) {
            continue;
        }

        parts.push_back(gpoint);
        prevGpoint = gpoint;
    }

    if(parts.empty()) {
        return nullptr;
    }

    GEOSCoordSeq ncs = GEOSCoordSeq_create_r(m_geosHandle.get(),
                                             static_cast<unsigned int>(parts.size()),
                                             2);
    unsigned int counter = 0;
    for(const OGRRawPoint& pt : parts) {
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, counter, pt.x);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, counter, pt.y);
        counter++;
    }

    return GEOSGeom_createLineString_r(m_geosHandle.get(), ncs);
}

GEOSGeom GEOSGeometryWrap::generalizeMultiLine(const GEOSGeom_t* geom,
                                               double step)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        CPLError(CE_Failure, CPLE_ObjectNull, "Geometry has no parts");
        return nullptr;
    }
    GEOSGeom parts[count];
    unsigned int counter = 0;
    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t* g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);
        GEOSGeom ng = generalizeLine(g, step);
        if(nullptr != ng) {
            parts[counter++] = ng;
        }
    }

    return  GEOSGeom_createCollection_r(m_geosHandle.get(), GEOS_MULTILINESTRING,
                                        parts, counter);
}

GEOSGeom GEOSGeometryWrap::generalizePolygon(const GEOSGeom_t* geom, double step)
{
    // TODO:
}

GEOSGeom GEOSGeometryWrap::generalizeMultiPolygon(const GEOSGeom_t* geom,
                                                  double step)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        CPLError(CE_Failure, CPLE_ObjectNull, "Geometry has no parts");
        return nullptr;
    }
    GEOSGeom parts[count];
    unsigned int counter = 0;
    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t* g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);
        GEOSGeom ng = generalizePolygon(g, step);
        if(nullptr != ng) {
            parts[counter++] = ng;
        }
    }

    return  GEOSGeom_createCollection_r(m_geosHandle.get(), GEOS_MULTIPOLYGON,
                                        parts, counter);
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
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 0, x);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 0, y);
        GEOSCoordSeq_setX_r(m_geosHandle.get(), ncs, 1, x + 1.5);
        GEOSCoordSeq_setY_r(m_geosHandle.get(), ncs, 1, y + 1.5);

        m_geom = GEOSGeom_createLineString_r(m_geosHandle.get(), ncs);
    }
    else if(type == GEOS_POLYGON) {
        // TODO:
        m_geom = g;
    }
}

void GEOSGeometryWrap::fillPointTile(GIntBig fid, const GEOSGeom_t* geom,
                                     VectorTileItemArray& vitemArray)
{
    VectorTileItem vitem;
    vitem.addId(fid);

    double x(0.0), y(0.0);
    GEOSGeomGetX_r(m_geosHandle.get(), geom, &x);
    GEOSGeomGetY_r(m_geosHandle.get(), geom, &y);
    SimplePoint pt = { static_cast<float>(x), static_cast<float>(y) };

    vitem.addPoint(pt);

    if(vitem.pointCount() > 0) {
        vitem.setValid(true);
        vitemArray.push_back(vitem);
    }
}

void GEOSGeometryWrap::fillMultiPointTile(GIntBig fid, const GEOSGeom_t* geom,
                                          VectorTileItemArray& vitemArray)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        return;
    }

    VectorTileItem vitem;
    vitem.addId(fid);

    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t* g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);
        double x(0.0), y(0.0);
        GEOSGeomGetX_r(m_geosHandle.get(), g, &x);
        GEOSGeomGetY_r(m_geosHandle.get(), g, &y);
        SimplePoint pt = { static_cast<float>(x), static_cast<float>(y) };
        vitem.addPoint(pt);
    }

    if(vitem.pointCount() > 0) {
        vitem.setValid(true);
        vitemArray.push_back(vitem);
    }
}

void GEOSGeometryWrap::fillLineTile(GIntBig fid, const GEOSGeom_t* geom,
                                    VectorTileItemArray& vitemArray)
{
    VectorTileItem vitem;
    vitem.addId(fid);

    const GEOSCoordSequence* cs = GEOSGeom_getCoordSeq_r(m_geosHandle.get(),
                                                         geom);
    unsigned int count = 0;
    GEOSCoordSeq_getSize_r(m_geosHandle.get(), cs, &count);

    double x(0.0), y(0.0);
    for(unsigned int i = 0; i < count; ++i) {
        GEOSCoordSeq_getX_r(m_geosHandle.get(), cs, i, &x);
        GEOSCoordSeq_getY_r(m_geosHandle.get(), cs, i, &y);
        SimplePoint pt = { static_cast<float>(x), static_cast<float>(y) };
        vitem.addPoint(pt);
    }

    if(vitem.pointCount() > 0) {
        vitem.setValid(true);
        vitemArray.push_back(vitem);
    }
}

void GEOSGeometryWrap::fillMultiLineTile(GIntBig fid, const GEOSGeom_t* geom,
                                         VectorTileItemArray& vitemArray)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        return;
    }

    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t* g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);
        fillLineTile(fid, g, vitemArray);
    }
}

void GEOSGeometryWrap::fillPolygonTile(GIntBig fid, const GEOSGeom_t* geom,
                                       VectorTileItemArray& vitemArray)
{
    // TODO:
}

void GEOSGeometryWrap::fillMultiPolygonTile(GIntBig fid, const GEOSGeom_t* geom,
                                            VectorTileItemArray& vitemArray)
{
    // TODO:
}

void GEOSGeometryWrap::fillCollectionTile(GIntBig fid, const GEOSGeom_t* geom,
                                          VectorTileItemArray& vitemArray)
{
    int count = GEOSGetNumGeometries_r(m_geosHandle.get(), geom);
    if(0 == count) {
        return;
    }

    for(int i = 0; i < count; ++i) {
        const GEOSGeom_t* g = GEOSGetGeometryN_r(m_geosHandle.get(), geom, i);

        switch(GEOSGeomTypeId_r(m_geosHandle.get(), g)) {
            case GEOS_LINESTRING:
                fillLineTile(fid, g, vitemArray);
                break;
            case GEOS_POLYGON:
                fillPolygonTile(fid, g, vitemArray);
                break;
            case GEOS_MULTILINESTRING:
                fillMultiLineTile(fid, g, vitemArray);
                break;
            case GEOS_MULTIPOLYGON:
                fillMultiPolygonTile(fid, g, vitemArray);
                break;
            case GEOS_GEOMETRYCOLLECTION:
                fillCollectionTile(fid, g, vitemArray);
                break;
            case GEOS_MULTIPOINT:
            case GEOS_POINT:
            case GEOS_LINEARRING:
            default:
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Expectred point/line/polygon/multipoint/multiline/multipolygon here");
                break;
        }
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

void GEOSGeometryWrap::fillTile(GIntBig fid, VectorTileItemArray& vitemArray)
{
    if(GEOSisEmpty_r(m_geosHandle.get(), m_geom) == 1) {
        return;
    }

    switch(type()) {
    case GEOS_POINT:
        fillPointTile(fid, m_geom, vitemArray);
        break;
    case GEOS_LINESTRING:
        fillLineTile(fid, m_geom, vitemArray);
        break;
    case GEOS_POLYGON:
        fillPolygonTile(fid, m_geom, vitemArray);
        break;
    case GEOS_MULTIPOINT:
        fillMultiPointTile(fid, m_geom, vitemArray);
        break;
    case GEOS_MULTILINESTRING:
        fillMultiLineTile(fid, m_geom, vitemArray);
        break;
    case GEOS_MULTIPOLYGON:
        fillMultiPolygonTile(fid, m_geom, vitemArray);
        break;
    case GEOS_GEOMETRYCOLLECTION:
        fillCollectionTile(fid, m_geom, vitemArray);
        break;
    case GEOS_LINEARRING:
    default:
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Expectred point/line/polygon/multipoint/multiline/multipolygon here");
        break;
    }
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
SimplePoint ngsGetMedianPoint(const SimplePoint& pt1, const SimplePoint& pt2)
{
    return {(pt2.x - pt1.x) / 2 + pt1.x, (pt2.y - pt1.y) / 2 + pt1.y};
}

OGRGeometry* ngsCreateGeometryFromGeoJson(const CPLJSONObject& json)
{
    return OGRGeometryFactory::createFromGeoJson(json);
}

bool ngsIsGeometryIntersectsEnvelope(const OGRGeometry& geometry, const Envelope env)
{
    OGREnvelope ogrEnv;
    geometry.getEnvelope(&ogrEnv);
    return ogrEnv.Intersects(env.toOgrEnvelope());
}



} // namespace ngs
