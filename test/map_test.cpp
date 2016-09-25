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
#include "featuredataset.h"
#include "mapstore.h"
#include "constants.h"
#include "mapview.h"

#include <memory>

static int counter = 0;

int ngsTestProgressFunc(unsigned int/* taskId*/, double /*complete*/,
                        const char* /*message*/, void* /*progressArguments*/) {
    counter++;
    return TRUE;
}

TEST(MapTests, TestCreate) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.createMap(DEFAULT_MAP_NAME, "unit test", DEFAULT_EPSG,
                                 DEFAULT_MIN_X, DEFAULT_MIN_Y, DEFAULT_MAX_X,
                                 DEFAULT_MAX_Y), 0);
    EXPECT_EQ(mapStore.mapCount(), 1);

    ngs::MapPtr defMap = mapStore.getMap (1);
    ASSERT_NE(defMap, nullptr);
    ngs::MapView * mapView = static_cast< ngs::MapView * >(defMap.get ());
    ngsRGBA color = mapView->getBackgroundColor ();
    EXPECT_EQ(color.R, 210);
    EXPECT_EQ(color.G, 245);
    EXPECT_EQ(color.B, 255);

    // ngmd - NextGIS map document
    EXPECT_EQ (mapView->save ("default.ngmd"), ngsErrorCodes::EC_SUCCESS);
}

TEST(MapTests, TestOpenMap) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    EXPECT_EQ(mapStore.mapCount(), 1);
}

TEST(MapTests, TestInitMap) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    EXPECT_EQ(mapStore.mapCount(), 1);
    EXPECT_NE(mapStore.initMap (2), ngsErrorCodes::EC_SUCCESS);
    EXPECT_EQ(mapStore.initMap (1), ngsErrorCodes::EC_SUCCESS);
}

TEST(MapTests, TestProject) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    ngs::MapPtr defMap = mapStore.getMap (1);
    ASSERT_NE(defMap, nullptr);

    // axis Y inverted
    EXPECT_EQ(mapStore.initMap (1), ngsErrorCodes::EC_SUCCESS);
    EXPECT_EQ(mapStore.setMapSize (1, 640, 480, true), ngsErrorCodes::EC_SUCCESS);

    ngs::MapView * mapView = static_cast< ngs::MapView * >(defMap.get ());
    OGREnvelope env;
    OGRRawPoint pt;
    OGRRawPoint wdPt;
    OGRRawPoint dwPt;


    // World is from (-1560, -1420) to (3560, 2420), 5120x3840
    env.MinX = -1560; env.MinY = -1420;
    env.MaxX = 3560; env.MaxY = 2420;
    mapView->setExtent (env);
    EXPECT_EQ(mapView->getScale (), 0.125);

    pt.x = -1560;
    pt.y = 2420;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 0);
    EXPECT_EQ(wdPt.y, 0);
    pt.x = 0;
    pt.y = 0;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, -1560);
    EXPECT_EQ(dwPt.y, 2420);

    pt.x = 3560;
    pt.y = 2420;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 640);
    EXPECT_EQ(wdPt.y, 0);
    pt.x = 640;
    pt.y = 0;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 3560);
    EXPECT_EQ(dwPt.y, 2420);

    pt.x = 3560;
    pt.y = -1420;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 640);
    EXPECT_EQ(wdPt.y, 480);
    pt.x = 640;
    pt.y = 480;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 3560);
    EXPECT_EQ(dwPt.y, -1420);

    pt.x = -1560;
    pt.y = -1420;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 0);
    EXPECT_EQ(wdPt.y, 480);
    pt.x = 0;
    pt.y = 480;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, -1560);
    EXPECT_EQ(dwPt.y, -1420);

    pt.x = 0;
    pt.y = 0;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 195);
    EXPECT_EQ(wdPt.y, 302.5);
    pt.x = 195;
    pt.y = 302.5;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 0);
    EXPECT_EQ(dwPt.y, 0);


    // axis Y is normal
    EXPECT_EQ(mapStore.setMapSize (1, 640, 480, false), ngsErrorCodes::EC_SUCCESS);

    // World is from (1000, 500) to (3560, 2420), 2560x1920
    env.MinX = 1000; env.MinY = 500;
    env.MaxX = 3560; env.MaxY = 2420;
    mapView->setExtent (env);
    EXPECT_EQ(mapView->getScale (), 0.25);

    pt.x = 1000;
    pt.y = 2420;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 0);
    EXPECT_EQ(wdPt.y, 480);
    pt.x = 0;
    pt.y = 480;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 1000);
    EXPECT_EQ(dwPt.y, 2420);

    pt.x = 3560;
    pt.y = 2420;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 640);
    EXPECT_EQ(wdPt.y, 480);
    pt.x = 640;
    pt.y = 480;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 3560);
    EXPECT_EQ(dwPt.y, 2420);

    pt.x = 3560;
    pt.y = 500;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 640);
    EXPECT_EQ(wdPt.y, 0);
    pt.x = 640;
    pt.y = 0;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 3560);
    EXPECT_DOUBLE_EQ(dwPt.y, 500);

    pt.x = 1000;
    pt.y = 500;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 0);
    EXPECT_EQ(wdPt.y, 0);
    pt.x = 0;
    pt.y = 0;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 1000);
    EXPECT_DOUBLE_EQ(dwPt.y, 500);

    pt.x = 0;
    pt.y = 0;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, -250);
    EXPECT_EQ(wdPt.y, -125);
    pt.x = -250;
    pt.y = -125;
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 0);
    // EXPECT_EQ(dwPt.y, 0);  // dwPt.y == 2.2737367544323206e-13


    // axis Y inverted
    EXPECT_EQ(mapStore.setMapSize (1, 480, 640, true), ngsErrorCodes::EC_SUCCESS);

    env.MinX = 0; env.MinY = 0;
    env.MaxX = 480; env.MaxY = 640;
    mapView->setExtent (env);
    EXPECT_EQ(mapView->getScale (), 1);

    pt.x = 0;
    pt.y = 0;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 0);
    EXPECT_EQ(wdPt.y, 640);
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 0);
    EXPECT_EQ(dwPt.y, 640);

    pt.x = 480;
    pt.y = 640;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 480);
    EXPECT_EQ(wdPt.y, 0);
    dwPt = mapView->displayToWorld (pt);
    EXPECT_EQ(dwPt.x, 480);
    EXPECT_EQ(dwPt.y, 0);

    EXPECT_EQ(mapStore.setMapSize (1, 640, 480, true), ngsErrorCodes::EC_SUCCESS);
    env.MinX = 0; env.MinY = 0;
    env.MaxX = 5120; env.MaxY = 3840;
    mapView->setExtent (env);
    pt.x = 0;
    pt.y = 0;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 0);
    EXPECT_EQ(wdPt.y, 480);

    env.MinX = -1560.0; env.MinY = -1420.0;
    env.MaxX = 3560.0; env.MaxY = 2420;
    mapView->setExtent (env);
    pt.x = -1560.0;
    pt.y = -1420.0;
    wdPt = mapView->worldToDisplay (pt);
    EXPECT_EQ(wdPt.x, 0);
    EXPECT_EQ(wdPt.y, 480);
}

TEST(MapTests, TestDrawing) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    /*unsigned char buffer[480 * 640 * 4];
    EXPECT_EQ(mapStore.initMap (0, buffer, 480, 640, true), ngsErrorCodes::SUCCESS);
    */
    ngs::MapPtr defMap = mapStore.getMap (1);
    ASSERT_NE(defMap, nullptr);
    ngs::MapView * mapView = static_cast< ngs::MapView * >(defMap.get ());
    mapView->setBackgroundColor ({255, 0, 0, 255}); // RED color
    /*counter = 0;
    mapView->draw(ngsTestProgressFunc, nullptr);
    CPLSleep(0.3);
    EXPECT_EQ(buffer[0], 255);
    EXPECT_EQ(buffer[1], 0);
    EXPECT_EQ(buffer[2], 0);
    EXPECT_GE(counter, 1);*/
    ngsRGBA color = mapView->getBackgroundColor ();
    EXPECT_EQ(color.R, 255);
    EXPECT_EQ(color.G, 0);
    EXPECT_EQ(color.B, 0);
}

TEST(MapTests, TestCreateLayer) {
    EXPECT_EQ(ngsInit(nullptr, nullptr), ngsErrorCodes::EC_SUCCESS);

    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    ngs::MapPtr defMap = mapStore.getMap (1);
    ASSERT_NE(defMap, nullptr);

    ngs::DataStorePtr storage = ngs::DataStore::create("./tmp/ngs.gpkg");
    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->loadDataset ("test", "./data/bld.shp", "", nullptr,
                                    ngsTestProgressFunc, nullptr),
              ngsErrorCodes::EC_SUCCESS);
    CPLSleep(0.6);
    EXPECT_GE(storage->datasetCount (), 0);
    EXPECT_GE(counter, 1);
    ngs::DatasetPtr data = storage->getDataset (0);
    ASSERT_NE(data, nullptr);
    bool isFeatureSet = data->type () & ngsDatasetType(Featureset);
    EXPECT_EQ(isFeatureSet, true);
    defMap->createLayer ("test_layer", data);
    EXPECT_EQ(defMap->save ("default.ngmd"), ngsErrorCodes::EC_SUCCESS);
}

TEST(MapTests, TestLoadLayer) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    ngs::MapPtr defMap = mapStore.getMap (1);
    ASSERT_NE(defMap, nullptr);
    EXPECT_NE (defMap->layerCount (), 0);
}

// TODO: add test create 2 maps and destroy them

TEST(MapTests, TestDeleteMap) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    ngs::MapPtr map = mapStore.getMap (1);
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->destroy (), ngsErrorCodes::EC_SUCCESS);
    EXPECT_EQ(CPLCheckForFile ((char*)"default.ngmd", nullptr), 0);
    ngs::DataStorePtr storage = ngs::DataStore::open("./tmp/ngs.gpkg");
    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->destroy (), ngsErrorCodes::EC_SUCCESS);
    ngsUninit();
}

