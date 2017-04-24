/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016,2017 NextGIS, <info@nextgis.com>
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

// stl
#include <limits>

#include "gl/view.h"
#include "ngstore/util/constants.h"
#include "util/notify.h"

namespace ngs {

constexpr unsigned char INVALID_MAPID = 0;

//------------------------------------------------------------------------------
// MapStore
//------------------------------------------------------------------------------
typedef std::unique_ptr<MapStore> MapStorePtr;
static MapStorePtr gMapStore;


MapStore::MapStore()
{
    // Add invalid map to 0 index
    m_maps.push_back(MapViewPtr());
}

unsigned char MapStore::createMap(const CPLString &name,
                                  const CPLString &description,
                                  unsigned short epsg, const Envelope &bounds)
{
    if(m_maps.size() >= std::numeric_limits<unsigned char>::max()) {
        return INVALID_MAPID;
    }
    m_maps.push_back(MapViewPtr(new GlView(name, description, epsg, bounds)));
    unsigned char result = static_cast<unsigned char>(m_maps.size() - 1);
    if(result != 0) {
        Notify::instance().onNotify(std::to_string(result).c_str(),
                                    ngsChangeCode::CC_CREATE_MAP);
    }
    return result;
}

unsigned char MapStore::openMap(MapFile * const file)
{
    if(nullptr == file || !file->open()) {
        return INVALID_MAPID;
    }

    MapViewPtr map = file->getMap();
    if(!map) {
        return INVALID_MAPID;
    }

    for(size_t i = 0; i < m_maps.size(); ++i) {
        if(m_maps[i] == map) {
            return static_cast<unsigned char>(i);
        }
    }

    m_maps.push_back(map);
    return static_cast<unsigned char>(m_maps.size() - 1);
}

bool MapStore::saveMap(unsigned char mapId, MapFile * const file)
{
    if(nullptr == file)
        return false;
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;

    return file->save(map);
}

bool MapStore::closeMap(unsigned char mapId)
{
    MapViewPtr map = getMap(mapId);
    if(!map) {
        return false;
    }
    if(map->close()) {
        m_maps[mapId].reset();
        return true;
    }

    return false;
}

MapViewPtr MapStore::getMap(unsigned char mapId) const
{    
    if(mapId > m_maps.size())
        return MapViewPtr();
    return m_maps[mapId];
}

bool MapStore::drawMap(unsigned char mapId, ngsDrawState state, const Progress &progress)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;
    return map->draw(state, progress);
}

ngsRGBA MapStore::getMapBackgroundColor(unsigned char mapId) const
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return {0,0,0,0};
    return map->getBackgroundColor ();
}

bool MapStore::setMapBackgroundColor(unsigned char mapId, const ngsRGBA &color)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;
    map->setBackgroundColor(color);
    Notify::instance().onNotify(std::to_string(mapId).c_str(),
                                ngsChangeCode::CC_CREATE_MAP);

    return true;
}

bool MapStore::setMapSize(unsigned char mapId, int width, int height,
                      bool YAxisInverted)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;
    map->setDisplaySize(width, height, YAxisInverted);
    return true;
}

bool MapStore::setMapCenter(unsigned char mapId, double x, double y)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;

    return map->setCenter(x, y);
}

ngsCoordinate MapStore::getMapCenter(unsigned char mapId) const
{
    ngsCoordinate out = {0.0, 0.0, 0.0};
    MapViewPtr map = getMap(mapId);
    if(!map)
        return out;

    OGRRawPoint ptWorld = map->getCenter();
    out.X = ptWorld.x;
    out.Y = ptWorld.y;

    return out;
}

bool MapStore::setMapScale(unsigned char mapId, double scale)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;

    return map->setScale(scale);
}

double MapStore::getMapScale(unsigned char mapId) const
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return 1.0;

    return map->getScale();
}

bool MapStore::setMapRotate(unsigned char mapId, ngsDirection dir, double rotate)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;

    return map->setRotate(dir, rotate);
}

double MapStore::getMapRotate(unsigned char mapId, ngsDirection dir) const
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return 0.0;

    return map->getRotate(dir);
}

ngsCoordinate MapStore::getMapCoordinate(unsigned char mapId,
                                         double x, double y) const
{
    ngsCoordinate out = { 0.0, 0.0, 0.0 };
    MapViewPtr map = getMap(mapId);
    if(!map)
        return out;

    OGRRawPoint pt = map->displayToWorld(OGRRawPoint(x, y));
    out.X = pt.x;
    if(map->getYAxisInverted())
        out.Y = pt.y;
    else
        out.Y = -pt.y; // NOTE: The Y axis orientation differs in matrix and view
    return out;
}

ngsPosition MapStore::getDisplayPosition(unsigned char mapId,
                                         double x, double y) const
{
    ngsPosition out = {0, 0};
    MapViewPtr map = getMap(mapId);
    if(!map)
        return out;

    OGRRawPoint pt = map->worldToDisplay(OGRRawPoint(x, y));
    out.X = pt.x;
    out.Y = pt.y;
    return out;
}

ngsCoordinate MapStore::getMapDistance(unsigned char mapId,
                                       double w, double h) const
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

ngsPosition MapStore::getDisplayLength(unsigned char mapId,
                                       double w, double h) const
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

size_t MapStore::getLayerCount(unsigned char mapId) const
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return 0;
    return map->layerCount();
}

LayerPtr MapStore::getLayer(unsigned char mapId, int layerId) const
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return nullptr;
    return map->getLayer(layerId);
}

int MapStore::createLayer(unsigned char mapId, const char *name,
                          Dataset const * dataset)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return NOT_FOUND;
    int result = map->createLayer(name, dataset);
    if(result != NOT_FOUND) {
        Notify::instance().onNotify(CPLSPrintf("%d#%d", mapId, result),
                                    ngsChangeCode::CC_CREATE_LAYER);
    }
    return result;
}

bool MapStore::deleteLayer(unsigned char mapId, Layer *layer)
{
    MapViewPtr map = getMap(mapId);
    if(!map)
        return false;
    return map->deleteLayer(layer);
}

unsigned char MapStore::invalidMapId()
{
    return INVALID_MAPID;
}

MapViewPtr MapStore::initMap()
{
    return MapViewPtr(new GlView);
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
