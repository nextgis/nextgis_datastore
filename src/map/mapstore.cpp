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
#include "api.h"
#include "constants.h"
#include "table.h"
#include "mapview.h"
#include "ogr_geometry.h"

#include <cmath>

using namespace ngs;

//------------------------------------------------------------------------------
// MapStore
//------------------------------------------------------------------------------

MapStore::MapStore() : m_notifyFunc(nullptr)
{

}

MapStore::~MapStore()
{

}

int MapStore::createMap(const CPLString &name, const CPLString &description,
                        unsigned short epsg, double minX, double minY,
                        double maxX, double maxY)
{
    MapPtr newMap(new MapView(name, description, epsg, minX, minY, maxX, maxY));
    m_maps.push_back (newMap);

    return static_cast<int>(m_maps.size () - 1);
}

int MapStore::createMap(const CPLString &name, const CPLString &description,
                        unsigned short epsg, double minX, double minY,
                        double maxX, double maxY, DataStorePtr dataStore)
{
    MapPtr newMap(new MapView(name, description, epsg, minX, minY, maxX, maxY,
                              dataStore));
    m_maps.push_back (newMap);

    return static_cast<int>(m_maps.size () - 1);
}

unsigned int MapStore::mapCount() const
{    
    return static_cast<unsigned int>(m_maps.size ());
}

int MapStore::openMap(const char *path)
{
    MapPtr newMap(new MapView());
    int nRes = newMap->open (path);
    if(nRes != ngsErrorCodes::SUCCESS)
        return NOT_FOUND;

    m_maps.push_back (newMap);
    return static_cast<int>(m_maps.size () - 1);
}

int MapStore::openMap(const char *path, DataStorePtr dataStore)
{
    MapPtr newMap(new MapView(dataStore));
    int nRes = newMap->open (path);
    if(nRes != ngsErrorCodes::SUCCESS)
        return NOT_FOUND;

    m_maps.push_back (newMap);
    return static_cast<int>(m_maps.size () - 1);

}

int MapStore::saveMap(unsigned int mapId, const char *path)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::SAVE_FAILED;
    return m_maps[mapId]->save (path);
}

int MapStore::closeMap(unsigned int mapId)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::CLOSE_FAILED;
    return m_maps[mapId]->close ();
}

MapPtr MapStore::getMap(unsigned int mapId)
{
    MapPtr map;
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return map;
    return m_maps[mapId];
}

void MapStore::setNotifyFunc(ngsNotifyFunc notifyFunc)
{
    m_notifyFunc = notifyFunc;
}

void MapStore::unsetNotifyFunc()
{
    m_notifyFunc = nullptr;
}

ngsRGBA MapStore::getMapBackgroundColor(unsigned int mapId)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return {0,0,0,0};
    return m_maps[mapId]->getBackgroundColor ();
}

int MapStore::setMapBackgroundColor(unsigned int mapId, const ngsRGBA &color)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::INVALID;
    return m_maps[mapId]->setBackgroundColor (color);
}

int MapStore::initMap(unsigned int mapId, void *buffer, int width, int height, bool isYAxisInverted)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::INVALID;
    return pMapView->initBuffer (buffer, width, height, isYAxisInverted);
}

int MapStore::drawMap(unsigned int mapId, ngsProgressFunc progressFunc,
                      void *progressArguments)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::INVALID;

    return pMapView->draw (progressFunc, progressArguments);
}

int MapStore::setMapDisplayCenter(unsigned int mapId, int x, int y)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::INVALID;

    OGRRawPoint ptDisplay(x, y);
    OGRRawPoint ptWorld = pMapView->displayToWorld(ptDisplay);

    return pMapView->setCenter(ptWorld.x, ptWorld.y);
}

int MapStore::getMapDisplayCenter(unsigned int mapId, int &x, int &y)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::INVALID;

    OGRRawPoint ptWorld = pMapView->getCenter();
    OGRRawPoint ptDisplay = pMapView->worldToDisplay(ptWorld);
    x = static_cast<int>(lround(ptDisplay.x));
    y = static_cast<int>(lround(ptDisplay.y));

    return ngsErrorCodes::SUCCESS;
}

int MapStore::setMapScale(unsigned int mapId, double scale)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::INVALID;

    return pMapView->setScale(scale);
}

int MapStore::getMapScale(unsigned int mapId, double &scale)
{
    if(mapId >= mapCount () || m_maps[mapId]->isDeleted ())
        return ngsErrorCodes::INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::INVALID;

    scale = pMapView->getScale();

    return ngsErrorCodes::SUCCESS;
}

void MapStore::onLowMemory()
{
    // free all cached maps
    m_maps.clear ();
}
