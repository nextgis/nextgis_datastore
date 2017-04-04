/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
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
#include "mapstore.h"

#include <cmath>
#include <limits>

#include "mapview.h"
#include "ogr_geometry.h"
#include "ngstore/api.h"
//#include "ngstore/util/constants.h"

namespace ngs {

constexpr unsigned char INVALID_MAPID = 0;

//------------------------------------------------------------------------------
// MapStore
//------------------------------------------------------------------------------
typedef std::unique_ptr<MapStore> MapStorePtr;
static MapStorePtr gMapStore;


MapStore::MapStore()
{
    m_maps.push_back(MapViewPtr());
}

unsigned char MapStore::createMap(const CPLString &name, const CPLString &description,
                        unsigned short epsg, double minX, double minY,
                        double maxX, double maxY)
{
    if(m_maps.size() >= std::numeric_limits<unsigned char>::max())
        return INVALID_MAPID;
    m_maps.push_back(MapViewPtr(new MapView(name, description, epsg, minX, minY,
                                        maxX, maxY)));
    return static_cast<unsigned char>(m_maps.size() - 1);
}

unsigned char MapStore::openMap(const char *path)
{
    MapViewPtr newMap(new MapView());
    if(newMap->open(path))
        return INVALID_MAPID;

    m_maps.push_back(newMap);
    return static_cast<unsigned char>(m_maps.size() - 1);
}

bool MapStore::saveMap(unsigned char mapId, const char *path)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;
    return map->save(path);
}

bool MapStore::closeMap(unsigned char mapId)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;

    if(map->close()) {
        m_maps[mapId] = MapViewPtr();
        return true;
    }

    return false;
}

MapViewPtr MapStore::getMap(unsigned char mapId)
{    
    if(mapId > m_maps.size())
        return MapViewPtr();
    return m_maps[mapId];
}

ngsRGBA MapStore::getMapBackgroundColor(unsigned char mapId)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return {0,0,0,0};
    return map->getBackgroundColor ();
}

void MapStore::setMapBackgroundColor(unsigned char mapId, const ngsRGBA &color)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return;
    map->setBackgroundColor(color);
}

bool MapStore::setMapSize(unsigned char mapId, int width, int height,
                      bool isYAxisInverted)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;
    MapView* pMapView = static_cast<MapView*>(map.get ());
    if(nullptr == pMapView)
        return false;
    pMapView->setDisplaySize(width, height, isYAxisInverted);
    return true;
}

//int MapStore::initMap(unsigned char mapId)
//{
//    if(!m_maps[mapId])
//        return ngsErrorCodes::EC_INVALID;
//    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
//    if(nullptr == pMapView)
//        return ngsErrorCodes::EC_INVALID;
//    return pMapView->initDisplay ();
//}

int MapStore::drawMap(unsigned char mapId, ngsDrawState state, ngsProgressFunc progressFunc,
                      void *progressArguments)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::EC_INVALID;

    return pMapView->draw (state, progressFunc, progressArguments);
}

int MapStore::setMapCenter(unsigned char mapId, double x, double y)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::EC_INVALID;

    return pMapView->setCenter(x, y) ? ngsErrorCodes::EC_SUCCESS :
                                       ngsErrorCodes::EC_SET_FAILED;
}

ngsCoordinate MapStore::getMapCenter(unsigned char mapId)
{
    ngsCoordinate out = {0.0, 0.0, 0.0};
    if(!m_maps[mapId])
        return out;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return out;

    OGRRawPoint ptWorld = pMapView->getCenter();
    out.X = ptWorld.x;
    out.Y = ptWorld.y;

    return out;
}

int MapStore::setMapScale(unsigned char mapId, double scale)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::EC_INVALID;

    return pMapView->setScale(scale) ? ngsErrorCodes::EC_SUCCESS :
                                       ngsErrorCodes::EC_SET_FAILED;
}

double MapStore::getMapScale(unsigned char mapId)
{
    if(!m_maps[mapId])
        return 1.0;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return 1.0;

    return pMapView->getScale();
}

int MapStore::setMapRotate(unsigned char mapId, ngsDirection dir, double rotate)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::EC_INVALID;

    return pMapView->setRotate(dir, rotate) ? ngsErrorCodes::EC_SUCCESS :
                                         ngsErrorCodes::EC_SET_FAILED;

}

double MapStore::getMapRotate(unsigned char mapId, ngsDirection dir)
{
    if(!m_maps[mapId])
        return 0.0;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return 0.0;

    return pMapView->getRotate (dir);
}

ngsCoordinate MapStore::getMapCoordinate(unsigned char mapId, double x, double y)
{
    ngsCoordinate out = { 0.0, 0.0, 0.0 };
    if(!m_maps[mapId])
        return out;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return out;

    OGRRawPoint pt = pMapView->displayToWorld (OGRRawPoint(x, y));
    out.X = pt.x;
    out.Y = pt.y;
    return out;
}

ngsPosition MapStore::getDisplayPosition(unsigned char mapId, double x, double y)
{
    ngsPosition out = {0, 0};
    if(!m_maps[mapId])
        return out;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return out;

    OGRRawPoint pt = pMapView->worldToDisplay (OGRRawPoint(x, y));
    out.X = pt.x;
    out.Y = pt.y;
    return out;
}

ngsCoordinate MapStore::getMapDistance(unsigned char mapId, double w, double h)
{
    ngsCoordinate out = { 0.0, 0.0, 0.0 };
    MapViewPtr map = getMap(mapId);
    if(!map)
        return out;

    OGRRawPoint beg = map->displayToWorld (OGRRawPoint(0, 0));
    OGRRawPoint end = map->displayToWorld (OGRRawPoint(w, h));

    out.X = end.x - beg.x;
    out.Y = end.y - beg.y;
    return out;
}

ngsPosition MapStore::getDisplayLength(unsigned char mapId, double w, double h)
{
    ngsPosition out = {0, 0};
    MapViewPtr map = getMap(mapId);
    if(!map)
        return out;

    OGRRawPoint beg = map->worldToDisplay(OGRRawPoint(0, 0));
    OGRRawPoint end = map->worldToDisplay(OGRRawPoint(w, h));
    out.X = (end.x - beg.x);
    out.Y = (end.y - beg.y);
    return out;
}

void MapStore::freeResources()
{
    // free all cached maps
    m_maps.clear ();
}

void MapStore::setInstance(MapStore *pointer)
{
    if(gMapStore && nullptr != pointer) // Can be initialized only once.
        return;
    gMapStore.reset(pointer);
}

MapStore* MapStore::getInstance()
{
    return gMapStore.get();
}

}
