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

#define INVALID_MAPID 0
//------------------------------------------------------------------------------
// MapStore
//------------------------------------------------------------------------------


MapStore::MapStore() : m_mapCounter(0), m_notifyFunc(nullptr)
{

}

MapStore::~MapStore()
{

}

unsigned char MapStore::createMap(const CPLString &name, const CPLString &description,
                        unsigned short epsg, double minX, double minY,
                        double maxX, double maxY)
{
    MapPtr newMap(new MapView(name, description, epsg, minX, minY, maxX, maxY));
    m_maps[++m_mapCounter] = newMap;
    return m_mapCounter;
}

unsigned char MapStore::createMap(const CPLString &name, const CPLString &description,
                        unsigned short epsg, double minX, double minY,
                        double maxX, double maxY, DataStorePtr dataStore)
{
    MapPtr newMap(new MapView(name, description, epsg, minX, minY, maxX, maxY,
                              dataStore));
    m_maps[++m_mapCounter] = newMap;
    newMap->setId (m_mapCounter);
    return m_mapCounter;
}

unsigned char MapStore::mapCount() const
{    
    return static_cast<unsigned char>(m_maps.size ());
}

unsigned char MapStore::openMap(const char *path)
{
    MapPtr newMap(new MapView());
    int nRes = newMap->open (path);
    if(nRes != ngsErrorCodes::EC_SUCCESS)
        return INVALID_MAPID;

    m_maps[++m_mapCounter] = newMap;
    newMap->setId (m_mapCounter);
    return m_mapCounter;
}

unsigned char MapStore::openMap(const char *path, DataStorePtr dataStore)
{
    MapPtr newMap(new MapView(dataStore));
    int nRes = newMap->open (path);
    if(nRes != ngsErrorCodes::EC_SUCCESS)
        return INVALID_MAPID;

    m_maps[++m_mapCounter] = newMap;
    newMap->setId (m_mapCounter);
    return m_mapCounter;
}

int MapStore::saveMap(unsigned char mapId, const char *path)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_SAVE_FAILED;
    return m_maps[mapId]->save (path);
}

int MapStore::closeMap(unsigned char mapId)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_CLOSE_FAILED;
    int nRes = m_maps[mapId]->close ();
    if(nRes == ngsErrorCodes::EC_SUCCESS)
        m_maps[mapId].reset ();
    return nRes;
}

MapPtr MapStore::getMap(unsigned char mapId)
{
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

ngsRGBA MapStore::getMapBackgroundColor(unsigned char mapId)
{
    if(!m_maps[mapId])
        return {0,0,0,0};
    return m_maps[mapId]->getBackgroundColor ();
}

int MapStore::setMapBackgroundColor(unsigned char mapId, const ngsRGBA &color)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_INVALID;
    return m_maps[mapId]->setBackgroundColor (color);
}

int MapStore::setMapSize(unsigned char mapId, int width, int height,
                      bool isYAxisInverted)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::EC_INVALID;
    pMapView->setDisplaySize (width, height, isYAxisInverted);
    return ngsErrorCodes::EC_SUCCESS;
}


int MapStore::initMap(unsigned char mapId)
{
    if(!m_maps[mapId])
        return ngsErrorCodes::EC_INVALID;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::EC_INVALID;
    return pMapView->initDisplay ();
}

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

ngsCoordinate MapStore::getMapCoordinate(unsigned char mapId, int x, int y)
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
    out.X = static_cast<int>(pt.x);
    out.Y = static_cast<int>(pt.y);
    return out;
}

ngsCoordinate MapStore::getMapDistance(unsigned char mapId, int w, int h)
{
    ngsCoordinate out = { 0.0, 0.0, 0.0 };
    if(!m_maps[mapId])
        return out;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return out;

    OGRRawPoint beg = pMapView->displayToWorld (OGRRawPoint(0, 0));
    OGRRawPoint end = pMapView->displayToWorld (OGRRawPoint(w, h));

    out.X = end.x - beg.x;
    out.Y = end.y - beg.y;
    return out;
}

ngsPosition MapStore::getDisplayLength(unsigned char mapId, double w, double h)
{
    ngsPosition out = {0, 0};
    if(!m_maps[mapId])
        return out;
    MapView* pMapView = static_cast<MapView*>(m_maps[mapId].get ());
    if(nullptr == pMapView)
        return out;

    OGRRawPoint beg = pMapView->worldToDisplay (OGRRawPoint(0, 0));
    OGRRawPoint end = pMapView->worldToDisplay (OGRRawPoint(w, h));
    out.X = static_cast<int>(end.x - beg.x);
    out.Y = static_cast<int>(end.y - beg.y);
    return out;
}

void MapStore::onLowMemory()
{
    // free all cached maps
    m_maps.clear ();
}
