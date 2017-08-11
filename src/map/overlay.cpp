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
{
    Settings& settings = Settings::instance();
    m_tolerancePx = settings.getDouble("map/overlay/edit/tolerance", TOLERANCE_PX);
}

GeometryPtr EditLayerOverlay::createGeometry(
        const OGRwkbGeometryType geometryType,
        const OGRRawPoint& geometryCenter)
{
    OGRRawPoint mapDist =
            m_map.getMapDistance(GEOMETRY_SIZE_PX, GEOMETRY_SIZE_PX);

    switch (OGR_GT_Flatten(geometryType)) {
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
            mpt->addGeometryDirectly(
                    new OGRPoint(geometryCenter.x - mapDist.x, geometryCenter.y - mapDist.y));
            mpt->addGeometryDirectly(
                    new OGRPoint(geometryCenter.x + mapDist.x, geometryCenter.y + mapDist.y));
            return GeometryPtr(mpt);
        }
        default: {
            return GeometryPtr();
        }
    }
}

long EditLayerOverlay::getGeometryPointIdByCoordinates(
        const OGRRawPoint& mapCoordinates) const
{
    if (!m_geometry) {
        return NOT_FOUND;
    }

    OGRRawPoint mapTolerance =
            m_map.getMapDistance(m_tolerancePx, m_tolerancePx);

    double minX = mapCoordinates.x - mapTolerance.x;
    double maxX = mapCoordinates.x + mapTolerance.x;
    double minY = mapCoordinates.y - mapTolerance.y;
    double maxY = mapCoordinates.y + mapTolerance.y;
    Envelope mapEnv(minX, minY, maxX, maxY);

    return getGeometryPointId(*m_geometry, mapEnv);
}

bool EditLayerOverlay::shiftPoint(long id, const OGRRawPoint& mapOffset)
{
    if (!m_geometry) {
        return false;
    }

    return shiftGeometryPoint(*m_geometry, id, mapOffset);
}

}  // namespace ngs
