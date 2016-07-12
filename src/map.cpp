/******************************************************************************
*  Project: NextGIS ...
*  Purpose: Application for ...
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2012-2016 NextGIS, info@nextgis.ru
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#include "map.h"
#include "mapstore.h"
#include "constants.h"
#include "api.h"
#include "table.h"

using namespace ngs;

//------------------------------------------------------------------------------
// Layer
//------------------------------------------------------------------------------

Layer::Layer()
{

}

GIntBig Layer::id() const
{
    return NOT_FOUND;
}


//------------------------------------------------------------------------------
// Map
//------------------------------------------------------------------------------

Map::Map(FeaturePtr feature, MapStore * mapstore) : m_mapstore(mapstore),
    m_deleted(false)
{
    m_name = feature->GetFieldAsString (MAP_NAME);
    m_epsg = static_cast<short>(feature->GetFieldAsInteger (MAP_EPSG));
    m_description = feature->GetFieldAsString (MAP_DESCRIPTION);
    m_minX = feature->GetFieldAsDouble (MAP_MIN_X);
    m_minY = feature->GetFieldAsDouble (MAP_MIN_Y);
    m_maxX = feature->GetFieldAsDouble (MAP_MAX_X);
    m_maxY = feature->GetFieldAsDouble (MAP_MAX_Y);
    m_id = feature->GetFID ();
    m_bkColor = ngsHEX2RGBA (feature->GetFieldAsInteger (MAP_BKCOLOR));

    int count = 0;
    const GIntBig* pLayersOrder = feature->GetFieldAsInteger64List (MAP_LAYERS,
                                                                    &count);
    loadLayers(pLayersOrder, count);
}

Map::Map(const string& name, const string& description, short epsg, double minX,
    double minY, double maxX, double maxY, MapStore *mapstore) :
    m_mapstore(mapstore), m_name(name), m_description(description), m_epsg(epsg),
    m_minX(minX), m_minY(minY), m_maxX(maxX), m_maxY(maxY), m_id(NOT_FOUND),
    m_deleted(false)
{
    m_bkColor = {0, 255, 0, 255};
}

Map::~Map()
{

}

string Map::name() const
{
    return m_name;
}

void Map::setName(const string &name)
{
    m_name = name;
}

string Map::description() const
{
    return m_description;
}

void Map::setDescription(const string &description)
{
    m_description = description;
}

short Map::epsg() const
{
    return m_epsg;
}

void Map::setEpsg(short epsg)
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

long Map::id() const
{
    return m_id;
}


vector<GIntBig> Map::getLayerOrder() const
{
    vector<GIntBig> out;
    for(LayerPtr layer : m_layers)
        out.push_back (layer->id());
    return out;
}

int Map::loadLayers(const GIntBig* pValues, int count)
{
    const Table* layers = m_mapstore->getLayersTable();
    for(int i = 0; i < count; ++i) {
        GIntBig id = pValues[i];
        FeaturePtr feature = layers->getFeature (id);
        switch(feature->GetFieldAsInteger (LAYER_TYPE)){

        }
    }
    return ngsErrorCodes::SUCCESS;
}

int Map::save()
{
    if(m_deleted)
        return ngsErrorCodes::SAVE_FAILED;
    return m_mapstore->storeMap (this);
}

int Map::destroy()
{
    int nRet = m_mapstore->destroyMap(m_id);
    if(nRet == ngsErrorCodes::SUCCESS)
        m_deleted = true;
    return nRet;
}

bool Map::isDeleted() const
{
    return m_deleted;
}

void Map::setId(long id)
{
    m_id = id;
}

ngsRGBA Map::getBackgroundColor() const
{
    return m_bkColor;
}

int Map::setBackgroundColor(const ngsRGBA &color)
{
    m_bkColor = color;
    return ngsErrorCodes::SUCCESS;
}
