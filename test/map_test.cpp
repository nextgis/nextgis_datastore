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

#include "test.h"
#include "datastore.h"
#include "mapstore.h"
#include "constants.h"

#include <memory>

TEST(MapTests, TestCreate) {
    ngs::DataStorePtr storage = std::make_shared<ngs::DataStore>(
                "./tmp", nullptr, nullptr);
    EXPECT_EQ(storage->create (), ngsErrorCodes::SUCCESS);
    ngs::MapStore mapStore(storage);
    EXPECT_EQ(mapStore.create(), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(mapStore.mapCount(), 1);
}

TEST(MapTests, TestOpenMap) {
    ngs::DataStorePtr storage = std::make_shared<ngs::DataStore>(
                "./tmp", nullptr, nullptr);
    EXPECT_EQ(storage->open (), ngsErrorCodes::SUCCESS);
    ngs::MapStore mapStore(storage);
    EXPECT_EQ(mapStore.mapCount(), 1);
}

TEST(MapTests, TestDeleteMap) {
    ngs::DataStorePtr storage = std::make_shared<ngs::DataStore>(
                "./tmp", nullptr, nullptr);
    EXPECT_EQ(storage->open (), ngsErrorCodes::SUCCESS);
    ngs::MapStore mapStore(storage);
    ngs::MapPtr map = mapStore.getMap ("default").lock ();
    EXPECT_EQ(map->destroy (), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(mapStore.mapCount(), 0);

    ngs::DatasetPtr dataset = storage->getDataset (LAYERS_TABLE_NAME).lock ();
    ASSERT_NE(dataset, nullptr);
    ngs::Table* pTable = static_cast<ngs::Table*>(dataset.get ());
    EXPECT_EQ(pTable->featureCount (), 0);

    EXPECT_EQ(storage->destroy (), ngsErrorCodes::SUCCESS);
}
