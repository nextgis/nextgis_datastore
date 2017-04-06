/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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

#include "map.h"
#include "mapstore.h"
#include "ngstore/util/constants.h"

namespace ngs {

constexpr ngsRGBA DEFAULT_MAP_BK = {210, 245, 255, 255};
constexpr const char* DEFAULT_MAP_NAME = "new map";
constexpr const char* MAP_NAME = "name";
constexpr const char* MAP_DESCRIPTION = "descript";
constexpr const char* MAP_LAYERS = "layers";
constexpr const char* MAP_RELATIVEPATHS = "relative_paths";
constexpr const char* MAP_EPSG = "epsg";
constexpr const char* MAP_MIN_X = "min_x";
constexpr const char* MAP_MIN_Y = "min_y";
constexpr const char* MAP_MAX_X = "max_x";
constexpr const char* MAP_MAX_Y = "max_y";
constexpr const char* MAP_BKCOLOR = "bk_color";

//------------------------------------------------------------------------------
// Map
//------------------------------------------------------------------------------

Map::Map() :
    m_name(DEFAULT_MAP_NAME),
    m_epsg(DEFAULT_EPSG),
    m_minX(DEFAULT_BOUNDS.getMinX()),
    m_minY(DEFAULT_BOUNDS.getMinY()),
    m_maxX(DEFAULT_BOUNDS.getMaxX()),
    m_maxY(DEFAULT_BOUNDS.getMaxY()),
    m_bkColor(DEFAULT_MAP_BK),
    m_relativePaths(true)
{
}

Map::Map(const CPLString& name, const CPLString& description, unsigned short epsg,
         double minX, double minY, double maxX, double maxY) :
    m_name(name),
    m_description(description),
    m_epsg(epsg),
    m_minX(minX),
    m_minY(minY),
    m_maxX(maxX),
    m_maxY(maxY),
    m_bkColor(DEFAULT_MAP_BK),
    m_relativePaths(true)
{
}

bool Map::open(MapFile * const mapFile)
{
    JSONDocument doc;
    if(doc.load(mapFile->getPath())) {
        return false;
    }

    JSONObject root = doc.getRoot();
    if(root.getType() == JSONObject::Type::Object) {
        m_name = root.getString (MAP_NAME, DEFAULT_MAP_NAME);
        m_description = root.getString (MAP_DESCRIPTION, "");
        m_relativePaths = root.getBool (MAP_RELATIVEPATHS, true);
        m_epsg = static_cast<unsigned short>(root.getInteger (MAP_EPSG,
                                                              DEFAULT_EPSG));
        m_minX = root.getDouble(MAP_MIN_X, DEFAULT_BOUNDS.getMinX());
        m_minY = root.getDouble(MAP_MIN_Y, DEFAULT_BOUNDS.getMinY());
        m_maxX = root.getDouble(MAP_MAX_X, DEFAULT_BOUNDS.getMaxX());
        m_maxY = root.getDouble(MAP_MAX_Y, DEFAULT_BOUNDS.getMaxY());
        m_bkColor = ngsHEX2RGBA(root.getInteger (MAP_BKCOLOR,
                                                 ngsRGBA2HEX(m_bkColor)));

        JSONArray layers = root.getArray("layers");
        for(int i = 0; i < layers.size(); ++i) {
            JSONObject layerConfig = layers[i];
            Layer::Type type = static_cast<Layer::Type>(
                        layerConfig.getInteger(LAYER_TYPE, 0));
            // load layer
            LayerPtr layer = createLayer(type);
            if(nullptr != layer) {
                if(layer->load(layerConfig, m_relativePaths ?
                               mapFile->getParent() : nullptr))
                    m_layers.push_back(layer);
            }
        }
    }

    return true;
}

bool Map::save(MapFile * const mapFile)
{
    JSONDocument doc;
    JSONObject root = doc.getRoot();

    root.add(MAP_NAME, m_name);
    root.add(MAP_DESCRIPTION, m_description);
    root.add(MAP_RELATIVEPATHS, m_relativePaths);
    root.add(MAP_EPSG, m_epsg);
    root.add(MAP_MIN_X, m_minX);
    root.add(MAP_MIN_Y, m_minY);
    root.add(MAP_MAX_X, m_maxX);
    root.add(MAP_MAX_Y, m_maxY);
    root.add(MAP_BKCOLOR, ngsRGBA2HEX(m_bkColor));

    JSONArray layers;
    for(LayerPtr layer : m_layers) {
        layers.add(layer->save(m_relativePaths ? mapFile->getParent() : nullptr));
    }
    root.add(MAP_LAYERS, layers);

    return doc.save(mapFile->getPath());
}

bool Map::close()
{
    m_layers.clear();
    return true;
}

//int Map::createLayer(const CPLString &name, DatasetPtr dataset)
//{
//    LayerPtr layer (new Layer(name, dataset));
//    m_layers.push_back (layer);
//    return ngsErrorCodes::EC_SUCCESS;
//}

LayerPtr Map::createLayer(Layer::Type /*type*/)
{
    return LayerPtr(new Layer);
}

}
