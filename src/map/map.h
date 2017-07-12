/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#ifndef NGSMAP_H
#define NGSMAP_H

#include "api_priv.h"

#include "layer.h"

#include "ds/datastore.h"

namespace ngs {

class MapFile;
class Map
{
public:
    Map();
    explicit Map(const CPLString& name, const CPLString& description, unsigned short epsg,
        const Envelope &bounds);
    virtual ~Map() = default;

    const CPLString &getName() const { return m_name; }
    void setName(const CPLString &name) { m_name = name; }
    const CPLString &getDescription() const { return m_description; }
    void setDescription(const CPLString &description) {
        m_description = description;
    }
    unsigned short getEpsg() const { return m_epsg; }
    void setEpsg(unsigned short epsg) { m_epsg = epsg; }
    void setBounds(const Envelope& bounds) {
        m_bounds = bounds;
    }
    Envelope getBounds() const { return m_bounds; }
    bool getRelativePaths() const { return m_relativePaths; }
    void setRelativePaths(bool relativePaths) { m_relativePaths = relativePaths; }

    bool open(MapFile * const mapFile);
    bool save(MapFile * const mapFile);

    virtual bool close();
    bool isClosed() const { return m_isClosed; }
    virtual ngsRGBA getBackgroundColor() const { return  m_bkColor; }
    virtual void setBackgroundColor(const ngsRGBA& color) { m_bkColor = color; }

    virtual int createLayer(const char* name, const ObjectPtr &object);
    size_t layerCount() const { return m_layers.size(); }
    LayerPtr getLayer(int layerId) const {
        if(layerId < 0) return nullptr;
        size_t layerIndex = static_cast<size_t>(layerId);
        if(layerIndex >= m_layers.size()) return nullptr;
        return m_layers[layerIndex];
    }
    virtual bool deleteLayer(Layer* layer);
    virtual bool reorderLayers(Layer* beforeLayer, Layer* movedLayer);

protected:
    virtual LayerPtr createLayer(const char* name = DEFAULT_LAYER_NAME,
                                 enum Layer::Type type = Layer::Type::Invalid);
    virtual bool openInternal(const JSONObject& root, MapFile * const mapFile);
    virtual bool saveInternal(JSONObject &root, MapFile * const mapFile);

protected:
    CPLString m_name;
    CPLString m_description;
    unsigned short m_epsg;
    Envelope m_bounds;
    std::vector<LayerPtr> m_layers;
    ngsRGBA m_bkColor;
    bool m_relativePaths, m_isClosed;
};

}
#endif // NGSMAP_H
