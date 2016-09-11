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

#include "map.h"
#include "mapstore.h"
#include "constants.h"

using namespace ngs;

//------------------------------------------------------------------------------
// Map
//------------------------------------------------------------------------------

Map::Map() : m_name(DEFAULT_MAP_NAME),
    m_epsg(DEFAULT_EPSG), m_minX(DEFAULT_MIN_X), m_minY(DEFAULT_MIN_Y),
    m_maxX(DEFAULT_MAX_X), m_maxY(DEFAULT_MAX_Y), m_relativePaths(true)
{
    m_bkColor = {210, 245, 255, 255};
}

Map::Map(const CPLString& name, const CPLString& description, unsigned short epsg,
         double minX, double minY, double maxX, double maxY) :
    m_name(name), m_description(description), m_epsg(epsg),
    m_minX(minX), m_minY(minY), m_maxX(maxX), m_maxY(maxY), m_relativePaths(true)
{
    m_bkColor = {210, 245, 255, 255};
}

Map::Map(DataStorePtr dataStore) : m_DataStore(dataStore), m_relativePaths(true)
{
    m_bkColor = {210, 245, 255, 255};
}

Map::Map(const CPLString &name, const CPLString &description,
         unsigned short epsg, double minX, double minY, double maxX, double maxY,
         DataStorePtr dataStore) :
    m_name(name), m_description(description), m_epsg(epsg),
    m_minX(minX), m_minY(minY), m_maxX(maxX), m_maxY(maxY),
    m_DataStore(dataStore), m_relativePaths(true)
{
    m_bkColor = {210, 245, 255, 255};
}

Map::~Map()
{
    close();
}

CPLString Map::name() const
{
    return m_name;
}

void Map::setName(const CPLString &name)
{
    m_name = name;
}

CPLString Map::description() const
{
    return m_description;
}

void Map::setDescription(const CPLString &description)
{
    m_description = description;
}

unsigned short Map::epsg() const
{
    return m_epsg;
}

void Map::setEpsg(unsigned short epsg)
{
    m_epsg = epsg;
}

double Map::minX() const
{
    return m_minX;
}

void Map::setMinX(double minX)
{
    m_minX = minX;
}

double Map::minY() const
{
    return m_minY;
}

void Map::setMinY(double minY)
{
    m_minY = minY;
}

double Map::maxX() const
{
    return m_maxX;
}

void Map::setMaxX(double maxX)
{
    m_maxX = maxX;
}

double Map::maxY() const
{
    return m_maxY;
}

void Map::setMaxY(double maxY)
{
    m_maxY = maxY;
}

int Map::open(const char *path)
{
    // TODO: need to switch to SAX json reader/writer, need abstract json class
    // for both json-c and new json support.
    m_path = path;

    JSONDocument doc;
    if(doc.load (path) != ngsErrorCodes::EC_SUCCESS)
        return ngsErrorCodes::EC_OPEN_FAILED;
    JSONObject root = doc.getRoot ();
    if(root.getType () == JSONObject::Type::Object) {

        if(root.getBool (MAP_SINGLESOURCE, false) && !m_DataStore)
            return ngsErrorCodes::EC_OPEN_FAILED;

        m_name = root.getString (MAP_NAME, DEFAULT_MAP_NAME);
        m_description = root.getString (MAP_DESCRIPTION, "");
        m_relativePaths = root.getBool (MAP_RELATIVEPATHS, true);
        m_epsg = static_cast<unsigned short>(root.getInteger (MAP_EPSG,
                                                              DEFAULT_EPSG));
        m_minX = root.getDouble (MAP_MIN_X, DEFAULT_MIN_X);
        m_minY = root.getDouble (MAP_MIN_Y, DEFAULT_MIN_Y);
        m_maxX = root.getDouble (MAP_MAX_X, DEFAULT_MAX_X);
        m_maxY = root.getDouble (MAP_MAX_Y, DEFAULT_MAX_Y);
        m_bkColor = ngsHEX2RGBA(root.getInteger (MAP_BKCOLOR,
                                                 ngsRGBA2HEX(m_bkColor)));

        JSONArray layers = root.getArray("layers");
        for(int i = 0; i < layers.size(); ++i) {
            JSONObject layerConfig = layers[i];
            Layer::Type type = static_cast<Layer::Type>(layerConfig.getInteger (
                                                            LAYER_TYPE, 0));
            // load layer
            LayerPtr layer = createLayer(type);
            if(nullptr != layer) {
                if(layer->load(layerConfig, m_DataStore, m_relativePaths ?
                           CPLGetPath(m_path) : "") == ngsErrorCodes::EC_SUCCESS)
                    m_layers.push_back (layer);
            }
        }
    }

    return ngsErrorCodes::EC_SUCCESS;
}

int Map::save(const char *path)
{
    JSONDocument doc;
    JSONObject root = doc.getRoot ();

    if(m_DataStore)
        root.add (MAP_SINGLESOURCE, true);
    root.add (MAP_NAME, m_name);
    root.add (MAP_DESCRIPTION, m_description);
    root.add (MAP_RELATIVEPATHS, m_relativePaths);
    root.add (MAP_EPSG, m_epsg);
    root.add (MAP_MIN_X, m_minX);
    root.add (MAP_MIN_Y, m_minY);
    root.add (MAP_MAX_X, m_maxX);
    root.add (MAP_MAX_Y, m_maxY);
    root.add (MAP_BKCOLOR, ngsRGBA2HEX(m_bkColor));

    JSONArray layers;
    for(LayerPtr layer : m_layers) {
        layers.add(layer->save(m_relativePaths ?
                                   CPLGetPath(m_path) : ""));
    }
    root.add ("layers", layers);

    return doc.save (CPLResetExtension (path, MAP_DOCUMENT_EXT));
}

int Map::destroy()
{
    if(m_path.empty ()) {
        return ngsErrorCodes::EC_SUCCESS;
    }


    int nRet = CPLUnlinkTree (m_path) == 0 ? ngsErrorCodes::EC_SUCCESS :
                                             ngsErrorCodes::EC_DELETE_FAILED;
    if(nRet == ngsErrorCodes::EC_SUCCESS) {
        close();
    }
    return nRet;
}

int Map::close()
{
    m_layers.clear ();
    return ngsErrorCodes::EC_SUCCESS;
}

ngsRGBA Map::getBackgroundColor() const
{
    return m_bkColor;
}

int Map::setBackgroundColor(const ngsRGBA &color)
{
    m_bkColor = color;
    return ngsErrorCodes::EC_SUCCESS;
}

int Map::createLayer(const CPLString &name, DatasetPtr dataset)
{
    LayerPtr layer (new Layer(name, dataset));
    m_layers.push_back (layer);
    return ngsErrorCodes::EC_SUCCESS;
}

size_t Map::layerCount() const
{
    return m_layers.size();
}

LayerPtr Map::createLayer(Layer::Type /*type*/)
{
    return LayerPtr(new Layer);
}

bool Map::getRelativePaths() const
{
    return m_relativePaths;
}

void Map::setRelativePaths(bool relativePaths)
{
    m_relativePaths = relativePaths;
}

unsigned char Map::getId() const
{
    return m_id;
}

void Map::setId(unsigned char id)
{
    m_id = id;
}
