/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
// stl
#include <memory>

#include "catalog/catalog.h"
#include "catalog/folder.h"
#include "ds/datastore.h"
#include "ds/geometry.h"
#include "map/gl/view.h"
#include "map/mapstore.h"
#include "map/mapview.h"
#include "map/overlay.h"
#include "ngstore/codes.h"
#include "ngstore/util/constants.h"

// TODO: Update/Fix unit test. Add GL offscreen rendering GL test
static int counter = 0;

constexpr const char* DEFAULT_MAP_NAME = "test map";
constexpr unsigned short DEFAULT_EPSG = 3857;
constexpr ngsRGBA DEFAULT_MAP_BK = {210, 245, 255, 255};

int ngsTestProgressFunc(double /*complete*/, const char* /*message*/,
                        void* /*progressArguments*/) {
    counter++;
    return TRUE;
}

TEST(MapTests, TestCreate) {
    char** options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), COD_SUCCESS);

    ngsListFree(options);


    ngs::MapStore mapStore;
    unsigned char mapId = mapStore.createMap(DEFAULT_MAP_NAME, "unit test",
                                             DEFAULT_EPSG, ngs::DEFAULT_BOUNDS);
    EXPECT_GE(mapId, 1);

    ngs::MapViewPtr defMap = mapStore.getMap(mapId);
    ASSERT_NE(defMap, nullptr);
    ngsRGBA color = defMap->backgroundColor();
    EXPECT_EQ(color.R, DEFAULT_MAP_BK.R);
    EXPECT_EQ(color.G, DEFAULT_MAP_BK.G);
    EXPECT_EQ(color.B, DEFAULT_MAP_BK.B);

    // ngmd - NextGIS map document
    ngs::CatalogPtr catalog = ngs::Catalog::instance();
    CPLString tmpDir = CPLFormFilename(CPLGetCurrentDir(), "tmp", nullptr);
    ngs::Folder::mkDir(tmpDir);
    ngs::ObjectPtr tmpDirObj = catalog->getObjectBySystemPath(tmpDir);
    ASSERT_NE(tmpDirObj, nullptr);
    ngs::ObjectContainer* tmpDirContainer =
            ngsDynamicCast(ngs::ObjectContainer, tmpDirObj);
    CPLString mapPath = CPLFormFilename(tmpDirObj->path().c_str(), "default", "ngmd");
    CPLString iconsPath = CPLFormFilename(CPLGetCurrentDir(), "data", nullptr);
    CPLString iconSet = CPLFormFilename(iconsPath, "tex", "png");
    defMap->addIconSet("simple", iconSet, true);

    ngs::MapFile mapFile(tmpDirContainer, "default.ngmd", mapPath);
    EXPECT_EQ(defMap->save(&mapFile), true);

    mapId = mapStore.openMap(&mapFile);
    defMap = mapStore.getMap(mapId);
    defMap->setBackgroundColor({1,2,3,4});
    EXPECT_EQ(defMap->save(&mapFile), true);

    ngs::Catalog::setInstance(nullptr);

    ngsUnInit();
}

TEST(MapTests, TestOpenMap) {
    char** options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), COD_SUCCESS);

    ngsListFree(options);

    ngs::MapStore mapStore;
    ngs::CatalogPtr catalog = ngs::Catalog::instance();
    CPLString tmpDir = CPLFormFilename(CPLGetCurrentDir(), "tmp", nullptr);
    ngs::ObjectPtr tmpDirObj = catalog->getObjectBySystemPath(tmpDir);
    ASSERT_NE(tmpDirObj, nullptr);
    ngs::ObjectContainer* tmpDirContainer =
            ngsDynamicCast(ngs::ObjectContainer, tmpDirObj);
    ASSERT_NE(tmpDirContainer->hasChildren(), 0);
    ngs::ObjectPtr mapFileObj = tmpDirContainer->getChild("default.ngmd");
    ngs::MapFile* mapFile = ngsDynamicCast(ngs::MapFile, mapFileObj);

    unsigned char mapId = mapStore.openMap(mapFile);
    EXPECT_GE(mapId, 1);
    ngs::MapViewPtr defMap = mapStore.getMap(mapId);
    ASSERT_NE(defMap, nullptr);
    EXPECT_EQ(defMap->hasIconSet("simple"), true);
    ngs::Catalog::setInstance(nullptr);

    auto iconData = defMap->iconSet("simple");
    ASSERT_NE(iconData.buffer, nullptr);
    EXPECT_EQ(iconData.buffer[0], 0);
    EXPECT_EQ(iconData.buffer[1], 0);
    EXPECT_EQ(iconData.buffer[2], 0);
    EXPECT_EQ(iconData.buffer[3], 0);

    ngsUnInit();
}

/*
TEST(MapTests, TestInitMap) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    EXPECT_EQ(mapStore.mapCount(), 1);
    EXPECT_NE(mapStore.initMap (2), ngsErrorCodes::EC_SUCCESS);
    EXPECT_EQ(mapStore.initMap (1), ngsErrorCodes::EC_SUCCESS);
}
*/


TEST(MapTests, TestProject)
{
    ngs::MapStore mapStore;
    unsigned char mapId = mapStore.createMap(
            DEFAULT_MAP_NAME, "", ngs::DEFAULT_EPSG, ngs::DEFAULT_BOUNDS);
    EXPECT_GE(mapId, 1);
    ngs::MapViewPtr defMap = mapStore.getMap(mapId);
    ASSERT_NE(defMap, nullptr);

    // axis Y inverted
    EXPECT_EQ(mapStore.setMapSize(mapId, 640, 480, true), true);

    ngs::MapView* mapView = static_cast<ngs::MapView*>(defMap.get());
    OGREnvelope env;
    OGRRawPoint pt;
    OGRRawPoint wdPt;
    OGRRawPoint dwPt;


    // World is from (-1560, -1420) to (3560, 2420), 5120x3840
    env.MinX = -1560;
    env.MinY = -1420;
    env.MaxX = 3560;
    env.MaxY = 2420;
    mapView->setExtent(env);
    EXPECT_EQ(mapView->getScale(), 0.125);

    pt.x = -1560;
    pt.y = 2420;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 0);
    EXPECT_DOUBLE_EQ(wdPt.y, 0);
    pt.x = 0;
    pt.y = 0;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, -1560);
    EXPECT_DOUBLE_EQ(dwPt.y, 2420);

    pt.x = 3560;
    pt.y = 2420;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 640);
    EXPECT_DOUBLE_EQ(wdPt.y, 0);
    pt.x = 640;
    pt.y = 0;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, 3560);
    EXPECT_DOUBLE_EQ(dwPt.y, 2420);

    pt.x = 3560;
    pt.y = -1420;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 640);
    EXPECT_DOUBLE_EQ(wdPt.y, 480);
    pt.x = 640;
    pt.y = 480;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, 3560);
    EXPECT_DOUBLE_EQ(dwPt.y, -1420);

    pt.x = -1560;
    pt.y = -1420;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 0);
    EXPECT_DOUBLE_EQ(wdPt.y, 480);
    pt.x = 0;
    pt.y = 480;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, -1560);
    EXPECT_DOUBLE_EQ(dwPt.y, -1420);

    pt.x = 0;
    pt.y = 0;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 195);
    EXPECT_DOUBLE_EQ(wdPt.y, 302.5);
    pt.x = 195;
    pt.y = 302.5;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_NEAR(dwPt.x, 0, 0.00000001);
    EXPECT_NEAR(dwPt.y, 0, 0.00000001);


    // axis Y is normal
    EXPECT_EQ(mapStore.setMapSize(mapId, 640, 480, false), true);

    // World is from (1000, 500) to (3560, 2420), 2560x1920
    env.MinX = 1000;
    env.MinY = 500;
    env.MaxX = 3560;
    env.MaxY = 2420;
    mapView->setExtent(env);
    EXPECT_DOUBLE_EQ(mapView->getScale(), 0.25);

    pt.x = 1000;
    pt.y = 2420;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 0);
    EXPECT_DOUBLE_EQ(wdPt.y, 480);
    pt.x = 0;
    pt.y = 480;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, 1000);
    EXPECT_DOUBLE_EQ(dwPt.y, 2420);

    pt.x = 3560;
    pt.y = 2420;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 640);
    EXPECT_DOUBLE_EQ(wdPt.y, 480);
    pt.x = 640;
    pt.y = 480;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, 3560);
    EXPECT_DOUBLE_EQ(dwPt.y, 2420);

    pt.x = 3560;
    pt.y = 500;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 640);
    EXPECT_DOUBLE_EQ(wdPt.y, 0);
    pt.x = 640;
    pt.y = 0;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, 3560);
    EXPECT_DOUBLE_EQ(dwPt.y, 500);

    pt.x = 1000;
    pt.y = 500;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 0);
    EXPECT_DOUBLE_EQ(wdPt.y, 0);
    pt.x = 0;
    pt.y = 0;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, 1000);
    EXPECT_DOUBLE_EQ(dwPt.y, 500);

    pt.x = 0;
    pt.y = 0;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, -250);
    EXPECT_DOUBLE_EQ(wdPt.y, -125);
    pt.x = -250;
    pt.y = -125;
    dwPt = mapView->displayToWorld(pt);
    EXPECT_NEAR(dwPt.x, 0, 0.00000001);
    EXPECT_NEAR(dwPt.y, 0, 0.00000001);


    // axis Y inverted
    EXPECT_EQ(mapStore.setMapSize(mapId, 480, 640, true), true);

    env.MinX = 0;
    env.MinY = 0;
    env.MaxX = 480;
    env.MaxY = 640;
    mapView->setExtent(env);
    EXPECT_DOUBLE_EQ(mapView->getScale(), 1);

    pt.x = 0;
    pt.y = 0;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 0);
    EXPECT_DOUBLE_EQ(wdPt.y, 640);
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, 0);
    EXPECT_DOUBLE_EQ(dwPt.y, 640);

    pt.x = 480;
    pt.y = 640;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 480);
    EXPECT_DOUBLE_EQ(wdPt.y, 0);
    dwPt = mapView->displayToWorld(pt);
    EXPECT_DOUBLE_EQ(dwPt.x, 480);
    EXPECT_DOUBLE_EQ(dwPt.y, 0);

    EXPECT_EQ(mapStore.setMapSize(mapId, 640, 480, true), true);
    env.MinX = 0;
    env.MinY = 0;
    env.MaxX = 5120;
    env.MaxY = 3840;
    mapView->setExtent(env);
    pt.x = 0;
    pt.y = 0;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 0);
    EXPECT_DOUBLE_EQ(wdPt.y, 480);

    env.MinX = -1560.0;
    env.MinY = -1420.0;
    env.MaxX = 3560.0;
    env.MaxY = 2420;
    mapView->setExtent(env);
    pt.x = -1560.0;
    pt.y = -1420.0;
    wdPt = mapView->worldToDisplay(pt);
    EXPECT_DOUBLE_EQ(wdPt.x, 0);
    EXPECT_DOUBLE_EQ(wdPt.y, 480);
}

/*
TEST(MapTests, TestDrawing) {
    ngs::MapStore mapStore;
    EXPECT_GE(mapStore.openMap ("default.ngmd"), 1);
    *//*unsigned char buffer[480 * 640 * 4];
    EXPECT_EQ(mapStore.initMap (0, buffer, 480, 640, true), ngsErrorCodes::SUCCESS);
    *//*
    ngs::MapPtr defMap = mapStore.getMap (1);
    ASSERT_NE(defMap, nullptr);
    ngs::MapView * mapView = static_cast< ngs::MapView * >(defMap.get ());
    mapView->setBackgroundColor ({255, 0, 0, 255}); // RED color
    *//* no drawing executed as we need to prepare GL surface and make it current
    counter = 0;
    mapView->draw(ngsTestProgressFunc, nullptr);
    CPLSleep(0.3);
    EXPECT_EQ(buffer[0], 255);
    EXPECT_EQ(buffer[1], 0);
    EXPECT_EQ(buffer[2], 0);
    EXPECT_GE(counter, 1);*//*
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
                                    ngsTestProgressFunc, nullptr), 1);
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
*/

TEST(MapTests, TestOverlayStruct) {
    ngs::OverlayPtr overlay;

    ngs::MapViewPtr mapView(new ngs::GlView());

    EXPECT_GE(mapView->overlayCount(), 1);
    overlay = mapView->getOverlay(MOT_EDIT);
    EXPECT_EQ(overlay->type(), MOT_EDIT);
    overlay = mapView->getOverlay(MOT_LOCATION);
    EXPECT_EQ(overlay->type(), MOT_LOCATION);

    // TODO: make full TestOverlayStruct
    //overlay = mapView->getOverlay(MOT_TRACK);
    //EXPECT_EQ(overlay->type(), MOT_TRACK);
}

TEST(MapTests, TestEditOverlay) {
    CPLString testDirName = "tmp";
    CPLString workDirName = "edit_overlay";
    CPLString storeName = "test_store";
    CPLString storeExt = "ngst";
    CPLString mapName = "test_map";
    CPLString mapExt = "ngmd";
    CPLString pointLayerName = "point_layer";
    CPLString multiPtLayerName = "multi_point_layer";

    CPLString tmpPath =
            ngsFormFileName(ngsGetCurrentDirectory(), testDirName, nullptr);
    if(!ngs::Folder::isExists(tmpPath))
        ngs::Folder::mkDir(tmpPath);

    CPLString workPath =
            ngsFormFileName(tmpPath, workDirName, nullptr);
    ASSERT_STRNE(workPath, "");

    // Init NGS.
    char** options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR", workPath);
    EXPECT_EQ(ngsInit(options), COD_SUCCESS);
    ngsListFree(options);

    // Get catalog.
    CPLString catalogPath = ngsCatalogPathFromSystem(workPath);
    ASSERT_STRNE(catalogPath, "");
    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath);

    // Create a store in the catalog.
    options = nullptr;
    options = ngsAddNameValue(
            options, "TYPE", CPLSPrintf("%d", CAT_CONTAINER_NGS));
    options = ngsAddNameValue(options, "CREATE_UNIQUE", "ON");
    EXPECT_EQ(ngsCatalogObjectCreate(catalog, storeName, options), COD_SUCCESS);
    ngsListFree(options);

    // Check the created store.
    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" << pathInfo[count].name
                  << '\n';
        ++count;
    }
    EXPECT_GE(count, 2);
    ngsFree(pathInfo);

    // Get the store.
    CPLString storePath = ngsCatalogPathFromSystem(
            ngsFormFileName(workPath, storeName, storeExt));
    CatalogObjectH store = ngsCatalogObjectGet(storePath);
    pathInfo = ngsCatalogObjectQuery(store, 0);
    ngsFree(pathInfo);

    // Create a new point layer in the store.
    options = nullptr;
    options = ngsAddNameValue(options, "TYPE", CPLSPrintf("%d", CAT_FC_GPKG));
    options = ngsAddNameValue(options, "GEOMETRY_TYPE", "POINT");

    EXPECT_EQ(ngsCatalogObjectCreate(store, pointLayerName, options), COD_SUCCESS);
    ngsListFree(options);

    // Check if the created layer exists.
    CatalogObjectH pointFc =
            ngsCatalogObjectGet(CPLString(storePath + "/" + pointLayerName));
    EXPECT_NE(pointFc, nullptr);

    // Create a new multi point layer in the store.
    options = nullptr;
    options = ngsAddNameValue(options, "TYPE", CPLSPrintf("%d", CAT_FC_GPKG));
    options = ngsAddNameValue(options, "GEOMETRY_TYPE", "MULTIPOINT");

    EXPECT_EQ(ngsCatalogObjectCreate(store, multiPtLayerName, options),
            COD_SUCCESS);
    ngsListFree(options);

    // Check if the created layer exists.
    CatalogObjectH multiPtFc =
            ngsCatalogObjectGet(CPLString(storePath + "/" + multiPtLayerName));
    EXPECT_NE(multiPtFc, nullptr);

    // Create a map.
    unsigned char mapId = ngsMapCreate(DEFAULT_MAP_NAME, "", ngs::DEFAULT_EPSG,
            ngs::DEFAULT_BOUNDS.minX(), ngs::DEFAULT_BOUNDS.minY(),
            ngs::DEFAULT_BOUNDS.maxX(), ngs::DEFAULT_BOUNDS.maxY());
    ASSERT_NE(mapId, 0);
    EXPECT_EQ(ngsMapSetBackgroundColor(mapId, DEFAULT_MAP_BK), COD_SUCCESS);
    EXPECT_EQ(ngsMapLayerCount(mapId), 0);

    // Add a created layers to the map.
    int pointLayerId = ngsMapCreateLayer(
            mapId, pointLayerName, CPLString(storePath + "/" + pointLayerName));
    EXPECT_EQ(pointLayerId, 0);
    EXPECT_EQ(ngsMapLayerCount(mapId), 1);

    int multiPtLayerId = ngsMapCreateLayer(mapId, multiPtLayerName,
            CPLString(storePath + "/" + multiPtLayerName));
    EXPECT_EQ(multiPtLayerId, 1);
    EXPECT_EQ(ngsMapLayerCount(mapId), 2);

    // Save map
    CPLString mapPath = catalogPath + "/" + mapName + "." + mapExt;
    EXPECT_EQ(ngsMapSave(mapId, mapPath), COD_SUCCESS);

    // Test editing.

    EXPECT_EQ(ngsFeatureClassCount(pointFc), 0);
    EXPECT_EQ(ngsOverlayGetVisible(mapId, MOT_EDIT), 0);

    LayerH layer = ngsMapLayerGet(mapId, pointLayerId);
    ASSERT_NE(layer, nullptr);

    EXPECT_EQ(ngsEditOverlayCreateGeometryInLayer(mapId, layer, 0), COD_SUCCESS);
    EXPECT_EQ(ngsOverlayGetVisible(mapId, MOT_EDIT), 1);

    EXPECT_EQ(ngsEditOverlayCancel(mapId), COD_SUCCESS);
    EXPECT_EQ(ngsOverlayGetVisible(mapId, MOT_EDIT), 0);

    EXPECT_EQ(ngsEditOverlayCreateGeometryInLayer(mapId, layer, 0), COD_SUCCESS);
    EXPECT_EQ(ngsOverlayGetVisible(mapId, MOT_EDIT), 1);

    EXPECT_NE(ngsEditOverlaySave(mapId), nullptr);
    EXPECT_EQ(ngsOverlayGetVisible(mapId, MOT_EDIT), 0);

    EXPECT_EQ(ngsFeatureClassCount(pointFc), 1);

    // Uninit NGS
    ngsUnInit();
}
