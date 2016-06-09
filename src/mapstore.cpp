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

using namespace ngs;

MapStore::MapStore(const DataStorePtr &dataStore) : m_dataStore(dataStore)
{

}

MapStore::~MapStore()
{

}

int MapStore::create()
{
    // TODO: move to map class
    DatasetPtr dataset = m_dataStore->getDataset (MAPS_TABLE_NAME);
    Table* pTable = static_cast<Table*>(dataset.get ());
    if(nullptr == pTable)
        return ngsErrorCodes::CREATE_MAP_FAILED;
    FeaturePtr feature = pTable->createFeature ();
    feature->SetField (MAP_NAME, MAP_DEFAULT_NAME);
    feature->SetField (MAP_EPSG, 3857);
    feature->SetField (MAP_DESCRIPTION, "The default map");
    // TODO: feature->SetField (MAP_CONTENT, data)
    feature->SetField (MAP_MIN_X, -20037508.34);
    feature->SetField (MAP_MIN_Y, -20037508.34);
    feature->SetField (MAP_MAX_X, 20037508.34);
    feature->SetField (MAP_MAX_Y, 20037508.34);

    return pTable->insertFeature (feature);
}

long MapStore::mapCount() const
{
    DatasetPtr dataset = m_dataStore->getDataset (MAPS_TABLE_NAME);
    Table* pTable = static_cast<Table*>(dataset.get ());
    if(nullptr == pTable)
        return 0;
    return pTable->featureCount ();
}
