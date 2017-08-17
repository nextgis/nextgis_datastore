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
#ifndef NGSGEOMETRY_H
#define NGSGEOMETRY_H

#include "cpl_json.h"
#include "ogrsf_frmts.h"
#include "ogr_geometry.h"

#include <array>
#include <memory>

#include "api_priv.h"
#include "ngstore/util/constants.h"

namespace ngs {
constexpr double BIG_VALUE = 10000000.0;

typedef std::shared_ptr< OGRGeometry > GeometryPtr;

class Envelope
{
public:
    Envelope();
    constexpr Envelope(double minX, double minY, double maxX, double maxY) :
        m_minX(minX),
        m_minY(minY),
        m_maxX(maxX),
        m_maxY(maxY) { }
    Envelope(const OGREnvelope& env) :
        m_minX(env.MinX),
        m_minY(env.MinY),
        m_maxX(env.MaxX),
        m_maxY(env.MaxY) { }
    void set(const OGREnvelope& env) {
        m_minX = env.MinX;
        m_minY = env.MinY;
        m_maxX = env.MaxX;
        m_maxY = env.MaxY;
    }

    void operator=(const OGREnvelope& env);

    bool isInit() const;
    void clear();
    OGRRawPoint center() const;
    void rotate(double angle);
    void setRatio(double ratio);
    void resize(double value);
    void move(double deltaX, double deltaY);
    constexpr double width() const { return m_maxX - m_minX; }
    constexpr double height() const { return m_maxY - m_minY; }
    GeometryPtr toGeometry(OGRSpatialReference * const spatialRef) const;
    OGREnvelope toOgrEnvelope() const;

    constexpr double minX() const { return m_minX; }
    constexpr double minY() const { return m_minY; }
    constexpr double maxX() const { return m_maxX; }
    constexpr double maxY() const { return m_maxY; }

    void setMinX(double minX) { m_minX = minX; }
    void setMinY(double minY) { m_minY = minY; }
    void setMaxX(double maxX) { m_maxX = maxX; }
    void setMaxY(double maxY) { m_maxY = maxY; }

    bool load(const CPLJSONObject& store, const Envelope& defaultValue);
    CPLJSONObject save() const;
    bool intersects(const Envelope &other) const;
    bool contains(Envelope const& other) const;
    const Envelope& merge( Envelope const& other );
    const Envelope& intersect( Envelope const& other );
    void fix();

protected:
    double m_minX, m_minY, m_maxX, m_maxY;
};

constexpr unsigned short DEFAULT_EPSG = 3857;

constexpr Envelope DEFAULT_BOUNDS = Envelope(-20037508.34, -20037508.34,
                                       20037508.34, 20037508.34);
constexpr Envelope DEFAULT_BOUNDS_X2 = Envelope(DEFAULT_BOUNDS.minX() * 2,
                                          DEFAULT_BOUNDS.minY() * 2,
                                          DEFAULT_BOUNDS.maxX() * 2,
                                          DEFAULT_BOUNDS.maxY() * 2);
constexpr Envelope DEFAULT_BOUNDS_Y2X4 = Envelope(DEFAULT_BOUNDS.minX() * 4,
                                          DEFAULT_BOUNDS.minY() * 2,
                                          DEFAULT_BOUNDS.maxX() * 4,
                                          DEFAULT_BOUNDS.maxY() * 2);

//    OGRGeometry* simplifyGeometry(const OGRGeometry* geometry, double distance);

typedef struct _normal {
    float x, y;
    bool operator==(const _normal& other) const { return isEqual(x, other.x) &&
                isEqual(y,other.y);}
} Normal, SimplePoint;

Normal ngsGetNormals(const SimplePoint &beg, const SimplePoint &end);

typedef struct _tile{
    int x, y;
    unsigned char z;
    char crossExtent;
    bool operator==(const struct _tile& other) const {
            return x == other.x && y == other.y && z == other.z &&
                    crossExtent == other.crossExtent;
    }
    bool operator<(const struct _tile& other) const {
        return x < other.x || (x == other.x && y < other.y) ||
                (x == other.x && y == other.y && z < other.z) ||
                (x == other.x && y == other.y && z == other.z &&
                 crossExtent < other.crossExtent);
    }
} Tile;

typedef struct _tileItem{
    Tile tile;
    Envelope env;
} TileItem;

OGRGeometry* ngsCreateGeometryFromGeoJson(const CPLJSONObject& json);

class PointId
{
public:
    explicit PointId(int pointId = NOT_FOUND,
            int ringId = NOT_FOUND,
            int geometryId = NOT_FOUND)
            : m_pointId(pointId)
            , m_ringId(ringId)
            , m_geometryId(geometryId)
    {
    }

    int pointId() const { return m_pointId; }
    int ringId() const { return m_ringId; }
    int geometryId() const { return m_geometryId; }
    bool isInit() const { return 0 <= pointId(); }

private:
    int m_pointId;
    int m_ringId; // 0 - exterior ring, 1+ - interior rings.
    int m_geometryId;
};

bool geometryIntersects(const OGRGeometry& geometry, const Envelope env);
PointId getGeometryPointId(
        const OGRGeometry& geometry, const Envelope env, OGRPoint* coordinates);
bool shiftGeometryPoint(OGRGeometry& geometry,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates);

} // namespace ngs


#endif // NGSGEOMETRY_H
