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
#ifndef NGSMAPSTORE_H
#define NGSMAPSTORE_H

#include "catalog/mapfile.h"

namespace ngs {


/**
 * @brief The MapStore class provides functionality to create, open, close, etc.
 * maps mostly for C API. In C++ one can work with map/mapview directly.
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
     * @return 0 if some error occures or new map identificator if success.
     */
    virtual unsigned char createMap(const CPLString& name,
                                    const CPLString& description,
                                    unsigned short epsg, const Envelope& bounds);
    virtual unsigned char openMap(MapFile * const file);
    virtual bool saveMap(unsigned char mapId, MapFile * const file);
    virtual bool closeMap(unsigned char mapId);
    virtual MapViewPtr getMap(unsigned char mapId) const;

    //
    void freeResources() { m_maps.clear (); }

    // Map manipulation
    bool drawMap(unsigned char mapId, enum ngsDrawState state,
                const Progress &progress = Progress());

    bool setMapSize(unsigned char mapId, int width, int height,
                   bool YAxisInverted);
    ngsRGBA getMapBackgroundColor(unsigned char mapId) const;
    bool setMapBackgroundColor(unsigned char mapId, const ngsRGBA& color);
    bool setMapCenter(unsigned char mapId, double x, double y);
    ngsCoordinate getMapCenter(unsigned char mapId) const;
    bool setMapScale(unsigned char mapId, double scale);
    double getMapScale(unsigned char mapId) const;
    bool setMapRotate(unsigned char mapId, enum ngsDirection dir, double rotate);
    double getMapRotate(unsigned char mapId, enum ngsDirection dir) const;
    ngsCoordinate getMapCoordinate(unsigned char mapId, double x, double y) const;
    ngsPosition getDisplayPosition(unsigned char mapId, double x, double y) const;
    ngsCoordinate getMapDistance(unsigned char mapId, double w, double h) const;
    ngsPosition getDisplayLength(unsigned char mapId, double w, double h) const;
    size_t getLayerCount(unsigned char mapId) const;
    LayerPtr getLayer(unsigned char mapId, int layerId) const;
    int createLayer(unsigned char mapId, const char* name, Dataset const * dataset);

    // static
public:
    static unsigned char invalidMapId();
    static MapViewPtr initMap();

public:
    static void setInstance(MapStore* pointer);
    static MapStore* getInstance();

protected:
    std::vector<MapViewPtr> m_maps;
};

}
#endif // NGSMAPSTORE_H
