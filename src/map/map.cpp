/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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

#include "map.h"

#include "catalog/mapfile.h"
#include "ds/simpledataset.h"
#include "ngstore/catalog/filter.h"
#include "ngstore/util/constants.h"
#include "util/error.h"


namespace ngs {

constexpr ngsRGBA DEFAULT_MAP_BK = {210, 245, 255, 255};
constexpr const char* DEFAULT_MAP_NAME = "new map";

constexpr const char* MAP_NAME_KEY = "name";
constexpr const char* MAP_DESCRIPTION_KEY = "descript";
constexpr const char* MAP_LAYERS_KEY = "layers";
constexpr const char* MAP_RELATIVEPATHS_KEY = "relative_paths";
constexpr const char* MAP_EPSG_KEY = "epsg";
constexpr const char* MAP_BKCOLOR_KEY = "bk_color";
constexpr const char* MAP_BOUNDS_KEY = "bounds";

//------------------------------------------------------------------------------
// Map
//------------------------------------------------------------------------------

Map::Map() :
    m_name(DEFAULT_MAP_NAME),
    m_epsg(DEFAULT_EPSG),
    m_bounds(DEFAULT_BOUNDS),
    m_bkColor(DEFAULT_MAP_BK),
    m_relativePaths(true)
{
}

Map::Map(const CPLString& name, const CPLString& description, unsigned short epsg,
         const Envelope &bounds) :
    m_name(name),
    m_description(description),
    m_epsg(epsg),
    m_bounds(bounds),
    m_bkColor(DEFAULT_MAP_BK),
    m_relativePaths(true)
{
}

bool Map::openInternal(const JSONObject& root, MapFile * const mapFile)
{
    m_name = root.getString(MAP_NAME_KEY, DEFAULT_MAP_NAME);
    m_description = root.getString(MAP_DESCRIPTION_KEY, "");
    m_relativePaths = root.getBool(MAP_RELATIVEPATHS_KEY, true);
    m_epsg = static_cast<unsigned short>(root.getInteger(MAP_EPSG_KEY,
                                                         DEFAULT_EPSG));
    m_bounds.load(root.getObject(MAP_BOUNDS_KEY), DEFAULT_BOUNDS);
    setBackgroundColor(ngsHEX2RGBA(root.getInteger (MAP_BKCOLOR_KEY,
                                             ngsRGBA2HEX(m_bkColor))));

    JSONArray layers = root.getArray("layers");
    for(int i = 0; i < layers.size(); ++i) {
        JSONObject layerConfig = layers[i];
        Layer::Type type = static_cast<Layer::Type>(
                    layerConfig.getInteger(LAYER_TYPE_KEY, 0));
        // load layer
        LayerPtr layer = createLayer(type);
        if(nullptr != layer) {
            if(layer->load(layerConfig, m_relativePaths ?
                           mapFile->getParent() : nullptr))
                m_layers.push_back(layer);
        }
    }

    return true;
}

bool Map::saveInternal(JSONObject& root, MapFile * const mapFile)
{
    root.add(MAP_NAME_KEY, m_name);
    root.add(MAP_DESCRIPTION_KEY, m_description);
    root.add(MAP_RELATIVEPATHS_KEY, m_relativePaths);
    root.add(MAP_EPSG_KEY, m_epsg);
    root.add(MAP_BOUNDS_KEY, m_bounds.save());
    root.add(MAP_BKCOLOR_KEY, ngsRGBA2HEX(m_bkColor));

    JSONArray layers;
    for(LayerPtr layer : m_layers) {
        layers.add(layer->save(m_relativePaths ? mapFile->getParent() : nullptr));
    }
    root.add(MAP_LAYERS_KEY, layers);

    return true;
}

bool Map::open(MapFile * const mapFile)
{
    JSONDocument doc;
    if(!doc.load(mapFile->getPath())) {
        return false;
    }

    JSONObject root = doc.getRoot();
    return openInternal(root, mapFile);
}

bool Map::save(MapFile * const mapFile)
{
    JSONDocument doc;
    JSONObject root = doc.getRoot();

    if(!saveInternal(root, mapFile))
        return false;

    return doc.save(mapFile->getPath());
}

bool Map::close()
{
    m_layers.clear();
    return true;
}

int Map::createLayer(const char * name, const ObjectPtr &object)
{
    LayerPtr layer;
    if(object->getType() == ngsCatalogObjectType::CAT_CONTAINER_SIMPLE) {
        SimpleDataset * const simpleDS = ngsDynamicCast(SimpleDataset, object);
        simpleDS->hasChildren();
        ObjectPtr internalObject = simpleDS->getInternalObject();
        if(internalObject) {
            return createLayer(name, internalObject);
        }
    }


    if(Filter::isFeatureClass(object->getType())) {
        FeatureLayer* newLayer = new FeatureLayer(name);
        FeatureClassPtr fc = std::dynamic_pointer_cast<FeatureClass>(object);
        newLayer->setFeatureClass(fc);
        layer.reset(newLayer);
    }
    else if(Filter::isRaster(object->getType())) {
        // TODO: Add raster layer support
    }

    if(layer) {
        m_layers.push_back(layer);
        return static_cast<int>(m_layers.size() - 1);
    }

    errorMessage(ngsErrorCode::EC_INVALID,
                 _("Source '%s' is not valid dataset"), object->getPath().c_str());

    return NOT_FOUND;
}

bool Map::deleteLayer(Layer *layer)
{
    for(auto it = m_layers.begin(); it != m_layers.end(); ++it) {
        if((*it).get() == layer) {
            m_layers.erase(it);
            return true;
        }
    }
    return false;
}

bool Map::reorderLayers(Layer *beforeLayer, Layer *movedLayer)
{
    LayerPtr movedLayerPtr;
    auto it = m_layers.begin(), before = m_layers.end();
    while(it != m_layers.end()) {
        if((*it).get() == beforeLayer) {
            before = it;
        }

        if((*it).get() == movedLayer) {
            movedLayerPtr = *it;
            bool invalidBefore = before == m_layers.end();
            it = m_layers.erase(it); // Changed m_layers.end() iterator (before) here.
            if(invalidBefore) {
                before = m_layers.end(); // Restore iterator if it has changed
            }

            if(!invalidBefore || beforeLayer == nullptr) { // Already have botch iterators or just push_back
                break;
            }
        }
        else {
            ++it;
        }
    }

    if(before != m_layers.end()) {
        m_layers.insert(before, movedLayerPtr);
     }
    else {
        m_layers.push_back(movedLayerPtr);
    }
    return true;
}

LayerPtr Map::createLayer(Layer::Type type)
{
    switch (type) {
    case Layer::Type::Vector:
        return LayerPtr(new FeatureLayer());
    default:
        return LayerPtr(new Layer);
    }
}

}
