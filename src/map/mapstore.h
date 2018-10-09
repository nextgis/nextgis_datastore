/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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
     * @return 0 if some error occurs or new map identifier if success.
     */
    virtual char createMap(const std::string &name,
                           const std::string &description,
                           unsigned short epsg, const Envelope &bounds);
    virtual char openMap(MapFile * const file);
    virtual bool saveMap(char mapId, MapFile * const file);
    virtual bool closeMap(char mapId);
    virtual MapViewPtr getMap(char mapId) const;

    //
    void freeResources() { m_maps.clear(); }

    // Map manipulation
    bool drawMap(char mapId, enum ngsDrawState state, const Progress &progress = Progress());
    void invalidateMap(char mapId, const Envelope &bounds);

    bool setMapSize(char mapId, int width, int height, bool YAxisInverted);
    ngsRGBA getMapBackgroundColor(char mapId) const;
    bool setMapBackgroundColor(char mapId, const ngsRGBA &color);
    bool setMapCenter(char mapId, double x, double y);
    ngsCoordinate getMapCenter(char mapId) const;
    bool setMapScale(char mapId, double scale);
    double getMapScale(char mapId) const;
    bool setMapRotate(char mapId, enum ngsDirection dir, double rotate);
    double getMapRotate(char mapId, enum ngsDirection dir) const;
    ngsCoordinate getMapCoordinate(char mapId, double x, double y) const;
    ngsPosition getDisplayPosition(char mapId, double x, double y) const;
    ngsCoordinate getMapDistance(char mapId, double w, double h) const;
    ngsPosition getDisplayLength(char mapId, double w, double h) const;
    size_t getLayerCount(char mapId) const;
    LayerPtr getLayer(char mapId, int layerId) const;
    int createLayer(char mapId, const std::string &name, const ObjectPtr &object);
    bool deleteLayer(char mapId, Layer *layer);
    bool reorderLayers(char mapId, Layer *beforeLayer, Layer *movedLayer);
    bool setOptions(char mapId, const Options &options);
    bool setExtentLimits(char mapId, const Envelope &extentLimits);
    OverlayPtr getOverlay(char mapId, enum ngsMapOverlayType type) const;
    bool setOverlayVisible(char mapId, int typeMask, bool visible);

    // static
public:
    static char invalidMapId();
    static MapViewPtr initMap();

public:
    static void setInstance(MapStore *pointer);
    static MapStore *instance();

protected:
    std::vector<MapViewPtr> m_maps;
};

}
#endif // NGSMAPSTORE_H
