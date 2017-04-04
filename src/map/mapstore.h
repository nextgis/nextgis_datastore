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
#ifndef NGSMAPSTORE_H
#define NGSMAPSTORE_H

#include "mapview.h"
#include "ngstore/api.h"

namespace ngs {



/**
 * @brief The MapStore class store maps with layers connected to datastore tables
 *        and styles (in memory storage)
 */
class MapStore
{
public:
    MapStore();
    virtual ~MapStore() = default;

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
    virtual unsigned char createMap(const CPLString& name, const CPLString& description,
                          unsigned short epsg, double minX, double minY,
                          double maxX, double maxY);
    virtual unsigned char openMap(const char* path);
    virtual bool saveMap(unsigned char mapId, const char* path);
    virtual bool closeMap(unsigned char mapId);
    virtual MapViewPtr getMap(unsigned char mapId);

    //
    void freeResources();

    // Map manipulation
    //    int initMap(unsigned char mapId);
    bool setMapSize(unsigned char mapId, int width, int height,
                   bool isYAxisInverted);
    int drawMap(unsigned char mapId, enum ngsDrawState state,
                ngsProgressFunc progressFunc, void* progressArguments = nullptr);
    ngsRGBA getMapBackgroundColor(unsigned char mapId);
    void setMapBackgroundColor(unsigned char mapId, const ngsRGBA& color);
    int setMapCenter(unsigned char mapId, double x, double y);
    ngsCoordinate getMapCenter(unsigned char mapId);
    int setMapScale(unsigned char mapId, double scale);
    double getMapScale(unsigned char mapId);
    int setMapRotate(unsigned char mapId, enum ngsDirection dir, double rotate);
    double getMapRotate(unsigned char mapId, enum ngsDirection dir);
    ngsCoordinate getMapCoordinate(unsigned char mapId, double x, double y);
    ngsPosition getDisplayPosition(unsigned char mapId, double x, double y);
    ngsCoordinate getMapDistance(unsigned char mapId, double w, double h);
    ngsPosition getDisplayLength(unsigned char mapId, double w, double h);

public:
    static void setInstance(MapStore* pointer);
    static MapStore* getInstance();

protected:
    std::vector<MapViewPtr> m_maps;
};

}
#endif // NGSMAPSTORE_H
