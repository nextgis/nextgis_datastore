/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
    m_relativePaths(true),
    m_isClosed(false)
{
}

Map::Map(const CPLString& name, const CPLString& description, unsigned short epsg,
         const Envelope &bounds) :
    m_name(name),
    m_description(description),
    m_epsg(epsg),
    m_bounds(bounds),
    m_bkColor(DEFAULT_MAP_BK),
    m_relativePaths(true),
    m_isClosed(false)
{
}

bool Map::openInternal(const CPLJSONObject &root, MapFile * const mapFile)
{
    m_name = root.GetString(MAP_NAME_KEY, DEFAULT_MAP_NAME);
    m_description = root.GetString(MAP_DESCRIPTION_KEY, "");
    m_relativePaths = root.GetBool(MAP_RELATIVEPATHS_KEY, true);
    m_epsg = static_cast<unsigned short>(root.GetInteger(MAP_EPSG_KEY,
                                                         DEFAULT_EPSG));
    m_bounds.load(root.GetObject(MAP_BOUNDS_KEY), DEFAULT_BOUNDS);
    setBackgroundColor(ngsHEX2RGBA(root.GetString(MAP_BKCOLOR_KEY,
                                             ngsRGBA2HEX(m_bkColor))));

    CPLJSONArray layers = root.GetArray("layers");
    CPLDebug("ngstore", "Opening map has %d layers", layers.Size());
    for(int i = 0; i < layers.Size(); ++i) {
        CPLJSONObject layerConfig = layers[i];
        Layer::Type type = static_cast<Layer::Type>(
                    layerConfig.GetInteger(LAYER_TYPE_KEY, 0));
        // load layer
        LayerPtr layer = createLayer(DEFAULT_LAYER_NAME, type);
        if(nullptr != layer) {
            if(layer->load(layerConfig, m_relativePaths ?
                           mapFile->parent() : nullptr)) {
                m_layers.push_back(layer);
            }
        }
    }

    m_isClosed = false;
    return true;
}

bool Map::saveInternal(CPLJSONObject &root, MapFile * const mapFile)
{
    root.Add(MAP_NAME_KEY, m_name);
    root.Add(MAP_DESCRIPTION_KEY, m_description);
    root.Add(MAP_RELATIVEPATHS_KEY, m_relativePaths);
    root.Add(MAP_EPSG_KEY, m_epsg);
    root.Add(MAP_BOUNDS_KEY, m_bounds.save());
    root.Add(MAP_BKCOLOR_KEY, ngsRGBA2HEX(m_bkColor));

    CPLJSONArray layers("layers");
    for(LayerPtr layer : m_layers) {
        layers.Add(layer->save(m_relativePaths ? mapFile->parent() : nullptr));
    }
    root.Add(MAP_LAYERS_KEY, layers);

    return true;
}

bool Map::open(MapFile * const mapFile)
{
    CPLJSONDocument doc;
    CPLString mapPath("/vsizip/");
    mapPath += mapFile->path();
    mapPath += "/data.json";
    if(!doc.Load(mapPath)) {
//    if(!doc.Load(mapFile->path())) {
        return false;
    }

    CPLJSONObject root = doc.GetRoot();
    return openInternal(root, mapFile);
}

bool Map::save(MapFile * const mapFile)
{
    CPLJSONDocument doc;
    CPLJSONObject root = doc.GetRoot();

    if(!saveInternal(root, mapFile))
        return false;

    CPLString mapPath("/vsizip/");
    mapPath += mapFile->path();
    mapPath += "/data.json";

    return doc.Save(mapPath);
    //return doc.Save(mapFile->path());
}

bool Map::close()
{
    m_layers.clear();
    m_isClosed = true;
    return true;
}

int Map::createLayer(const char* name, const ObjectPtr &object)
{
    LayerPtr layer;
    if(object->type() == CAT_CONTAINER_SIMPLE) {
        SimpleDataset * const simpleDS = ngsDynamicCast(SimpleDataset, object);
        simpleDS->hasChildren();
        ObjectPtr internalObject = simpleDS->internalObject();
        if(internalObject) {
            return createLayer(name, internalObject);
        }
    }

    if(Filter::isFeatureClass(object->type())) {
        layer = createLayer(name, Layer::Type::Vector);
        FeatureLayer* newFCLayer = ngsStaticCast(FeatureLayer, layer);
        FeatureClassPtr fc = std::dynamic_pointer_cast<FeatureClass>(object);
        newFCLayer->setFeatureClass(fc);
    }
    else if(Filter::isRaster(object->type())) {
        layer = createLayer(name, Layer::Type::Raster);
        RasterLayer* newRasterLayer = ngsStaticCast(RasterLayer, layer);
        RasterPtr raster = std::dynamic_pointer_cast<Raster>(object);
        if(!raster->isOpened()) {
            raster->open(GDAL_OF_SHARED|GDAL_OF_READONLY|GDAL_OF_VERBOSE_ERROR);
        }
        newRasterLayer->setRaster(raster);
    }

    if(layer) {
        m_layers.push_back(layer);
        return static_cast<int>(m_layers.size() - 1);
    }

    errorMessage(COD_INVALID,
                 _("Source '%s' is not a valid dataset"), object->path().c_str());

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

            if(!invalidBefore || beforeLayer == nullptr) { // Already have both iterators or just push_back
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

LayerPtr Map::createLayer(const char* name, Layer::Type type)
{
    switch (type) {
    case Layer::Type::Vector:
        return LayerPtr(new FeatureLayer(name));
    case Layer::Type::Raster:
        return LayerPtr(new RasterLayer(name));
    case Layer::Type::Group:
    case Layer::Type::Invalid:
        return LayerPtr(new Layer);
    }
    return LayerPtr();
}

}
