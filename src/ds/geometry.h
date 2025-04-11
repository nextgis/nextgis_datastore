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
#ifndef NGSGEOMETRY_H
#define NGSGEOMETRY_H

// gdal
#include "cpl_json.h"
#include "ogrsf_frmts.h"
#include "ogr_geometry.h"

// std
#include <array>
#include <memory>
#include <set>

#include "api_priv.h"
#include "ngstore/util/constants.h"
#include "util/buffer.h"
#include "coordinatetransformation.h"

namespace ngs {

constexpr double BIG_VALUE = 100000000.0; // 100 000 000
constexpr float BIG_VALUE_F = 100000000.0f; // 100 000 000

class GeometryPtr : public std::shared_ptr<OGRGeometry>
{
public:
    GeometryPtr(OGRGeometry *geom);
    GeometryPtr();
    GeometryPtr &operator=(OGRGeometry *geom);
    operator OGRGeometry*() const;
};

class Envelope
{
public:
    Envelope();
    constexpr Envelope(double minX, double minY, double maxX, double maxY) :
        m_minX(minX),
        m_minY(minY),
        m_maxX(maxX),
        m_maxY(maxY)
    { }
    Envelope(const OGREnvelope &env);
    void set(const OGREnvelope &env);

    void operator=(const OGREnvelope &env);

    bool isInit() const;
    void clear();
    OGRRawPoint center() const;
    void rotate(double angle);
    void setRatio(double ratio);
    void resize(double value);
    void move(double deltaX, double deltaY);
    constexpr double width() const { return m_maxX - m_minX; }
    constexpr double height() const { return m_maxY - m_minY; }
    GeometryPtr toGeometry(SpatialReferencePtr spatialRef) const;
    OGREnvelope toOgrEnvelope() const;

    constexpr double minX() const { return m_minX; }
    constexpr double minY() const { return m_minY; }
    constexpr double maxX() const { return m_maxX; }
    constexpr double maxY() const { return m_maxY; }

    void setMinX(double minX) { m_minX = minX; }
    void setMinY(double minY) { m_minY = minY; }
    void setMaxX(double maxX) { m_maxX = maxX; }
    void setMaxY(double maxY) { m_maxY = maxY; }

    bool load(const CPLJSONObject &store, const Envelope &defaultValue);
    CPLJSONObject save() const;
    bool intersects(const Envelope &other) const;
    bool contains(Envelope const &other) const;
    const Envelope &merge( Envelope const &other );
    const Envelope &intersect( Envelope const &other );
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


OGRGeometry *ngsCreateGeometryFromGeoJson(const CPLJSONObject &json);

bool ngsIsGeometryIntersectsEnvelope(const OGRGeometry &geometry,
                                    const Envelope &env);

class GEOSContextHandlePtr : public std::shared_ptr<struct GEOSContextHandle_HS>
{
public:
    GEOSContextHandlePtr() : shared_ptr(OGRGeometry::createGEOSContext(),
                                        OGRGeometry::freeGEOSContext) {}
};

class GEOSGeometryWrap;
using  GEOSGeometryPtr = std::shared_ptr<GEOSGeometryWrap>;
class GEOSGeometryWrap
{
public:
    explicit GEOSGeometryWrap(GEOSGeom geom, GEOSContextHandlePtr handle);
    explicit GEOSGeometryWrap(OGRGeometry *geom);
    ~GEOSGeometryWrap();
    GEOSGeom geom() const { return m_geom; }
    int type() const;
    GEOSGeometryPtr clip(const Envelope &env) const;
    void simplify(double step);
    bool isValid() const { return m_geom != nullptr; }
    double distance(double x, double y) const;
    bool intersects(double x, double y) const;

private:
    GEOSGeom generalizePoint(const GEOSGeom_t *geom, double step);
    GEOSGeom generalizeMultiPoint(const GEOSGeom_t *geom, double step);
    GEOSGeom generalizeLine(const GEOSGeom_t *geom, double step, bool isRing = false);
    GEOSGeom generalizeMultiLine(const GEOSGeom_t *geom, double step);
    GEOSGeom generalizePolygon(const GEOSGeom_t *geom, double step);
    GEOSGeom generalizeMultiPolygon(const GEOSGeom_t *geom, double step);
    void setCentroid(int type);

private:
    GEOSGeom m_geom;
    GEOSContextHandlePtr m_geosHandle;
};
                                    

} // namespace ngs


#endif // NGSGEOMETRY_H
