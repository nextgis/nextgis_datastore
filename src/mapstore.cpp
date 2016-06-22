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
#include "mapstore.h"
#include "api.h"
#include "constants.h"
#include "table.h"
#include "mapview.h"

using namespace ngs;

MapStore::MapStore(const DataStorePtr &dataStore) : m_datastore(dataStore)
{

}

MapStore::~MapStore()
{

}

int MapStore::create()
{
    Map newMap(MAP_DEFAULT_NAME, "The default map", 3857, -20037508.34,
               -20037508.34, 20037508.34, 20037508.34, this);

    return newMap.save();
}

GIntBig MapStore::mapCount() const
{
    DatasetPtr dataset = m_datastore->getDataset (MAPS_TABLE_NAME).lock ();
    if(nullptr == dataset)
        return 0;
    Table* pTable = static_cast<Table*>(dataset.get ());
    if(nullptr == pTable)
        return 0;
    return pTable->featureCount ();
}

MapWPtr MapStore::getMap(const char *name)
{
    MapPtr map;
    auto it = m_maps.find (name);
    if( it != m_maps.end ()){
        if(!it->second->isDeleted ()){
            return it->second;
        }
        else{
            return map;
        }
    }
    DatasetPtr dataset = m_datastore->getDataset (MAPS_TABLE_NAME).lock ();
    if(nullptr == dataset)
        return MapWPtr();
    Table* pTable = static_cast<Table*>(dataset.get ());
    if(nullptr == pTable)
        return MapWPtr();
    FeaturePtr feature;
    pTable->reset();
    while( (feature = pTable->nextFeature()) != nullptr ) {
        if(EQUAL(feature->GetFieldAsString (MAP_NAME), name)){
            map.reset (static_cast<Map*>(new MapView(feature, this)));
            m_maps[map->name ()] = map;
            return map;
        }
    }
    return map;
}

MapWPtr MapStore::getMap(int index)
{
    DatasetPtr dataset = m_datastore->getDataset (MAPS_TABLE_NAME).lock ();
    if(nullptr == dataset)
        return MapWPtr();
    Table* pTable = static_cast<Table*>(dataset.get ());
    if(nullptr == pTable)
        return MapWPtr();

    FeaturePtr feature = pTable->getFeature (index);
    return getMap (feature->GetFieldAsString (MAP_NAME));
}

int MapStore::storeMap(Map *map)
{
    DatasetPtr dataset = m_datastore->getDataset (MAPS_TABLE_NAME).lock ();
    if(nullptr == dataset)
        return ngsErrorCodes::SAVE_FAILED;
    Table* pTable = static_cast<Table*>(dataset.get ());
    if(nullptr == pTable)
        return ngsErrorCodes::SAVE_FAILED;

    long id = map->id ();
    if(id == NOT_FOUND){
        FeaturePtr feature = pTable->createFeature ();
        feature->SetField (MAP_NAME, map->name ().c_str ());
        feature->SetField (MAP_EPSG, map->epsg ());
        feature->SetField (MAP_DESCRIPTION, map->description ().c_str ());
        vector<GIntBig> order = map->getLayerOrder ();
        GIntBig *pOrderArray = new GIntBig[order.size ()];
        int count = 0;
        for(GIntBig val : order)
            pOrderArray[count++] = val;
        feature->SetField (MAP_LAYERS, count, pOrderArray);
        delete [] pOrderArray;
        feature->SetField (MAP_MIN_X, map->minX ());
        feature->SetField (MAP_MIN_Y, map->minY ());
        feature->SetField (MAP_MAX_X, map->maxX ());
        feature->SetField (MAP_MAX_Y, map->maxY ());

        int nRes = pTable->insertFeature (feature);
        map->setId(feature->GetFID ());

        if(nRes == ngsErrorCodes::SUCCESS) {
            // TODO: notify map changed
        }
        return nRes;

    } else{
        FeaturePtr feature = pTable->getFeature (id);
        feature->SetField (MAP_NAME, map->name ().c_str ());
        feature->SetField (MAP_EPSG, map->epsg ());
        feature->SetField (MAP_DESCRIPTION, map->description ().c_str ());
        vector<GIntBig> order = map->getLayerOrder ();
        GIntBig *pOrderArray = new GIntBig[order.size ()];
        int count = 0;
        for(GIntBig val : order)
            pOrderArray[count++] = val;
        feature->SetField (MAP_LAYERS, count, pOrderArray);
        delete [] pOrderArray;
        feature->SetField (MAP_MIN_X, map->minX ());
        feature->SetField (MAP_MIN_Y, map->minY ());
        feature->SetField (MAP_MAX_X, map->maxX ());
        feature->SetField (MAP_MAX_Y, map->maxY ());

        int nRes = pTable->updateFeature (feature);
        if(nRes == ngsErrorCodes::SUCCESS) {
            // TODO: notify map changed
        }
        return nRes;
    }
}

bool MapStore::isNameValid(const string &name) const
{
    if(name.size () < 4 || name.size () >= NAME_FIELD_LIMIT)
        return false;
    if((name[0] == 'n' || name[0] == 'N') &&
       (name[1] == 'g' || name[1] == 'G') &&
       (name[2] == 's' || name[2] == 'S') &&
       (name[3] == '_'))
        return false;
    if(m_maps.find (name) != m_maps.end ())
        return false;
    string statement("SELECT count(*) FROM " MAPS_TABLE_NAME " WHERE "
                     LAYER_NAME " = '");
    statement += name + "'";
    ResultSetPtr tmpLayer = m_datastore->executeSQL ( statement );
    FeaturePtr feature = tmpLayer->GetFeature (0);
    if(feature->GetFieldAsInteger (0) > 0)
        return false;
    return true;
}

int MapStore::destroyMap(GIntBig mapId)
{
    // TODO: notify listeneres about changes
    DatasetPtr dataset = m_datastore->getDataset (MAPS_TABLE_NAME).lock ();
    if(nullptr == dataset)
        return ngsErrorCodes::DELETE_FAILED;
    Table* pTable = static_cast<Table*>(dataset.get ());
    if(nullptr == pTable)
        return ngsErrorCodes::DELETE_FAILED;


    CPLErrorReset();
    // delete layers
    CPLString statement = CPLOPrintf("DELETE FROM " LAYERS_TABLE_NAME " WHERE "
                     MAP_ID " = %lld", mapId);
    m_datastore->executeSQL ( statement );

    if (CPLGetLastErrorNo() == CE_None) {
        // delete map
        return pTable->deleteFeature (mapId);
    }

    return ngsErrorCodes::DELETE_FAILED;
}

const Table *MapStore::getLayersTable() const
{
    DatasetPtr dataset = m_datastore->getDataset (LAYERS_TABLE_NAME).lock ();
    if(nullptr == dataset)
        return nullptr;
    return static_cast<Table*>(dataset.get ());
}

int MapStore::initMap(const char *name, void *buffer, int width, int height)
{
    MapWPtr mapw = getMap (name);
    if(mapw.expired ())
        return ngsErrorCodes::INIT_FAILED;
    MapPtr map = mapw.lock ();
    MapView* pMapView = static_cast<MapView*>(map.get ());
    if(nullptr == pMapView)
        return ngsErrorCodes::INIT_FAILED;
    return pMapView->initBuffer (buffer, width, height);
}

void MapStore::onLowMemory()
{
    // free all cached maps memory
    m_maps.clear ();

    if(nullptr != m_datastore)
        m_datastore->onLowMemory ();
}
