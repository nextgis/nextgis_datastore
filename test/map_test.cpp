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

int ngsTestProgressFunc(double /*complete*/, const char* /*message*/,
                        void* /*progressArguments*/) {
    counter++;
    return TRUE;
}

TEST(MapTests, TestCreate) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.createMap(DEFAULT_MAP_NAME, "unit test", DEFAULT_EPSG,
                                 DEFAULT_MIN_X, DEFAULT_MIN_Y, DEFAULT_MAX_X,
                                 DEFAULT_MAX_Y), 0);
    EXPECT_EQ(mapStore.mapCount(), 1);

    ngs::MapPtr defMap = mapStore.getMap (0);
    ASSERT_NE(defMap, nullptr);
    ngs::MapView * mapView = static_cast< ngs::MapView * >(defMap.get ());
    ngsRGBA color = mapView->getBackgroundColor ();
    EXPECT_EQ(color.R, 210);
    EXPECT_EQ(color.G, 245);
    EXPECT_EQ(color.B, 255);

    // ngmd - NextGIS map document
    EXPECT_EQ (mapView->save ("default.ngmd"), ngsErrorCodes::SUCCESS);
}

TEST(MapTests, TestOpenMap) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 0);
    EXPECT_EQ(mapStore.mapCount(), 1);
}

TEST(MapTests, TestInitMap) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 0);
    EXPECT_EQ(mapStore.mapCount(), 1);
    EXPECT_NE(mapStore.initMap (1, nullptr, 640, 480), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(mapStore.initMap (0, nullptr, 640, 480), ngsErrorCodes::SUCCESS);

}

TEST(MapTests, TestProject) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 0);
    EXPECT_EQ(mapStore.initMap (0, nullptr, 480, 640), ngsErrorCodes::SUCCESS);
    ngs::MapPtr defMap = mapStore.getMap (0);
    ASSERT_NE(defMap, nullptr);
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

TEST(MapTests, TestDrawing) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 0);
    unsigned char buffer[480 * 640 * 4];
    EXPECT_EQ(mapStore.initMap (0, buffer, 480, 640), ngsErrorCodes::SUCCESS);
    ngs::MapPtr defMap = mapStore.getMap (0);
    ASSERT_NE(defMap, nullptr);
    ngs::MapView * mapView = static_cast< ngs::MapView * >(defMap.get ());
    mapView->setBackgroundColor ({255, 0, 0, 255}); // RED color
    counter = 0;
    mapView->draw(ngsTestProgressFunc, nullptr);
    CPLSleep(0.3);
    EXPECT_EQ(buffer[0], 255);
    EXPECT_EQ(buffer[1], 0);
    EXPECT_EQ(buffer[2], 0);
    EXPECT_GE(counter, 1);
    ngsRGBA color = mapView->getBackgroundColor ();
    EXPECT_EQ(color.R, 255);
    EXPECT_EQ(color.G, 0);
    EXPECT_EQ(color.B, 0);
}

TEST(MapTests, TestDeleteMap) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 0);
    ngs::MapPtr map = mapStore.getMap (0);
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(map->destroy (), ngsErrorCodes::SUCCESS);    
    EXPECT_EQ(CPLCheckForFile ((char*)"default.ngmd", nullptr), 0);
}

