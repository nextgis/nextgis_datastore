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
#ifndef MAPSTORE_H
#define MAPSTORE_H

#include "datastore.h"
#include "map.h"
#include "api.h"

namespace ngs {

/**
 * @brief The MapStore class store maps with layers connected to datastore tables
 *        and styles (in memory storage)
 */
class MapStore
{
    friend class Map;
public:
    MapStore();
    virtual ~MapStore();

    /**
     * @brief createMap Create new map
     * @param name Map name
     * @param description Map description
     * @param epsg EPSG code
     * @param minX minimum x
     * @param minY minimum y
     * @param maxX maximum x
     * @param maxY maximum y
     * @return -1 if some error occures or map Id
     */
    virtual int createMap(const CPLString& name, const CPLString& description,
                          unsigned short epsg, double minX, double minY,
                          double maxX, double maxY);
    /**
     * @brief map count in storage
     * @return map count
     */
    virtual unsigned int mapCount() const;
    virtual int openMap(const char* path);
    virtual int saveMap(unsigned int mapId, const char* path);
    virtual MapPtr getMap(unsigned int mapId);
    int initMap(unsigned int mapId, void *buffer, int width, int height, bool isYAxisInverted);
    int drawMap(unsigned int mapId, ngsProgressFunc progressFunc,
                void* progressArguments = nullptr);
    void onLowMemory();
    void setNotifyFunc(ngsNotifyFunc notifyFunc);
    void unsetNotifyFunc();
    ngsRGBA getMapBackgroundColor(unsigned int mapId);
    int setMapBackgroundColor(unsigned int mapId, const ngsRGBA& color);
    int setMapDisplayCenter(unsigned int mapId, int x, int y);
    int getMapDisplayCenter(unsigned int mapId, int &x, int &y);
    int setMapScale(unsigned int mapId, double scale);
    int getMapScale(unsigned int mapId, double &scale);

protected:
    vector<MapPtr> m_maps;
    ngsNotifyFunc m_notifyFunc;
};

typedef shared_ptr<MapStore> MapStorePtr;

}
#endif // MAPSTORE_H
