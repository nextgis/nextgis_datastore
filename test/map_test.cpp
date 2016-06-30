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
#include "mapview.h"

#include <memory>

static int counter = 0;

void ngsTestNotifyFunc(enum ngsSourceCodes /*src*/, const char* /*table*/, long /*row*/,
                       enum ngsChangeCodes /*operation*/) {
    counter++;
}

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

TEST(MapTests, TestInitMap) {
    ngs::DataStorePtr storage = std::make_shared<ngs::DataStore>(
                "./tmp", nullptr, nullptr);
    EXPECT_EQ(storage->open (), ngsErrorCodes::SUCCESS);
    ngs::MapStore mapStore(storage);
    EXPECT_NE(mapStore.initMap ("test", nullptr, 640, 480), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(mapStore.initMap ("default", nullptr, 640, 480), ngsErrorCodes::SUCCESS);
}

TEST(MapTests, TestProject) {

    ngs::DataStorePtr storage = std::make_shared<ngs::DataStore>(
                "./tmp", nullptr, nullptr);
    EXPECT_EQ(storage->open (), ngsErrorCodes::SUCCESS);
    ngs::MapStore mapStore(storage);
    EXPECT_EQ(mapStore.initMap ("default", nullptr, 480, 640), ngsErrorCodes::SUCCESS);
    ngs::MapWPtr map = mapStore.getMap ("default");
    EXPECT_EQ(map.expired (), false);
    ngs::MapPtr defMap = map.lock ();
    ngs::MapView * mapView = static_cast< ngs::MapView * >(defMap.get ());
    OGREnvelope env;
    OGRRawPoint pt;
    env.MinX = 0; env.MinY = 0;
    env.MaxX = 480; env.MaxY = 640;
    mapView->setExtent (env);
    EXPECT_EQ(mapView->getScale (), 1);
    pt.x = 0;
    pt.y = 0;
    OGRRawPoint wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 0);
    EXPECT_EQ(wdPt.y, 0);
    OGRRawPoint dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 0);
    EXPECT_EQ(dwPt.y, 0);

    pt.x = 480;
    pt.y = 640;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 480);
    EXPECT_EQ(wdPt.y, 640);
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 480);
    EXPECT_EQ(dwPt.y, 640);
}

TEST(MapTests, TestDeleteMap) {
    ngs::DataStorePtr storage = std::make_shared<ngs::DataStore>(
                "./tmp", nullptr, nullptr);
    EXPECT_EQ(storage->open (), ngsErrorCodes::SUCCESS);
    ngs::MapStore mapStore(storage);
    counter = 0;
    mapStore.setNotifyFunc (ngsTestNotifyFunc);

    ngs::MapPtr map = mapStore.getMap ("default").lock ();
    EXPECT_EQ(map->destroy (), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(mapStore.mapCount(), 0);

    ngs::DatasetPtr dataset = storage->getDataset (LAYERS_TABLE_NAME).lock ();
    ASSERT_NE(dataset, nullptr);
    ngs::Table* pTable = static_cast<ngs::Table*>(dataset.get ());
    EXPECT_EQ(pTable->featureCount (), 0);
    EXPECT_GE(counter, 1);

    EXPECT_EQ(storage->destroy (), ngsErrorCodes::SUCCESS);
}
