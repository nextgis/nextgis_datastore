/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
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
#ifndef NGSMAP_H
#define NGSMAP_H

#include "api_priv.h"

#include "layer.h"

#include "ds/datastore.h"
#include "ngstore/codes.h"

namespace ngs {

class MapFile;
class Map
{
public:
    Map();
    explicit Map(const std::string& name, const std::string& description,
                 unsigned short epsg, const Envelope &bounds);
    virtual ~Map() = default;

    const std::string &name() const { return m_name; }
    void setName(const std::string &name) { m_name = name; }
    const std::string &description() const { return m_description; }
    void setDescription(const std::string &description) {
        m_description = description;
    }
    unsigned short epsg() const { return m_epsg; }
    void setEpsg(unsigned short epsg) { m_epsg = epsg; }
    void setBounds(const Envelope &bounds) {
        m_bounds = bounds;
    }
    Envelope bounds() const { return m_bounds; }
    bool isRelativePaths() const { return m_relativePaths; }
    void setRelativePaths(bool relativePaths) { m_relativePaths = relativePaths; }

    bool open(MapFile * const mapFile);
    bool save(MapFile * const mapFile);

    virtual bool close();
    bool isClosed() const { return m_isClosed; }
    virtual ngsRGBA backgroundColor() const { return  m_bkColor; }
    virtual void setBackgroundColor(const ngsRGBA &color) { m_bkColor = color; }

    size_t layerCount() const { return m_layers.size(); }
    LayerPtr getLayer(int layerId) const;
    virtual int createLayer(const std::string &name, const ObjectPtr &object);
    virtual bool deleteLayer(Layer *layer);
    virtual bool reorderLayers(Layer *beforeLayer, Layer *movedLayer);

protected:
    virtual LayerPtr createLayer(const std::string &name = DEFAULT_LAYER_NAME,
                                 enum Layer::Type type = Layer::Type::Invalid);
    virtual bool openInternal(const CPLJSONObject &root, MapFile * const mapFile);
    virtual bool saveInternal(CPLJSONObject &root, MapFile * const mapFile);

protected:
    std::string m_name;
    std::string m_description;
    unsigned short m_epsg;
    Envelope m_bounds;
    std::vector<LayerPtr> m_layers;
    ngsRGBA m_bkColor;
    bool m_relativePaths, m_isClosed;
};

}
#endif // NGSMAP_H
