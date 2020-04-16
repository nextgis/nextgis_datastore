/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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

// gdal
#include "cpl_conv.h"
#include "cpl_json.h"


#include "ds/datastore.h"

TEST(StoreTests, TestJSONSAXParser) {
    initLib();

    char **options = nullptr;
    options = ngsListAddNameValue(options, "MAX_RETRY", "20");
    options = ngsListAddNameValue(options, "RETRY_DELAY", "5");
    options = ngsListAddNameValue(options, "UNSAFESSL", "ON");
    resetCounter();
    CPLJSONDocument doc;
    EXPECT_EQ(doc.LoadUrl("https://sandbox.nextgis.com/api/component/pyramid/pkg_version",
                          options, ngsGDALProgressFunc, nullptr), true);
    ngsListFree(options);

    CPLJSONObject obj = doc.GetRoot();
    CPLString ngwVersion = obj.GetString("nextgisweb", "0");
    EXPECT_STRNE(ngwVersion, "0");

    ngsUnInit();
}


TEST(MIStoreTests, TestCreate) {
    initLib();

    CatalogObjectH mistore = createMIStore("test_mistore");
    ASSERT_NE(mistore, nullptr);

    // Create feature class
    char **options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_FC_MAPINFO_TAB);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsListAddNameValue(options, "DESCRIPTION", "Test Feature Class");
    options = ngsListAddNameValue(options, "GEOMETRY_TYPE", "LINESTRING");
    options = ngsListAddNameValue(options, "FIELD_COUNT", "3");
    options = ngsListAddNameValue(options, "FIELD_0_TYPE", "INTEGER");
    options = ngsListAddNameValue(options, "FIELD_0_NAME", "id");
    options = ngsListAddNameValue(options, "FIELD_0_ALIAS", "идентификатор");
    options = ngsListAddNameValue(options, "FIELD_1_TYPE", "STRING");
    options = ngsListAddNameValue(options, "FIELD_1_NAME", "desc");
    options = ngsListAddNameValue(options, "FIELD_1_ALIAS", "описание");
    options = ngsListAddNameValue(options, "FIELD_2_TYPE", "DATE_TIME");
    options = ngsListAddNameValue(options, "FIELD_2_NAME", "date");
    options = ngsListAddNameValue(options, "FIELD_2_ALIAS", "Это дата");
    options = ngsListAddNameValue(options, "ENCODING", "CP1251");
    CatalogObjectH miFC = ngsCatalogObjectCreate(mistore, "test_fc", options);
    ngsListFree(options);
    options = nullptr;
    ASSERT_NE(miFC, nullptr);

    // After created, reopen as read only
    EXPECT_STREQ(ngsCatalogObjectProperty(miFC, "read_only", "ON", "nga"), "OFF");
    EXPECT_EQ(ngsCatalogObjectSetProperty(miFC, "read_only", "ON", "nga"), COD_SUCCESS);
    EXPECT_STREQ(ngsCatalogObjectProperty(miFC, "read_only", "OFF", "nga"), "ON");

    ngsCatalogObjectInfo *pathInfo = ngsCatalogObjectQuery(mistore, 0);
    ASSERT_NE(pathInfo, nullptr);
    std::string mistorePath = ngsCatalogObjectPath(mistore);
    int count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << mistorePath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 1);
    ngsFree(pathInfo);

    // Delete
    EXPECT_EQ(ngsCatalogObjectDelete(mistore), COD_SUCCESS);

    ngsUnInit();
}

TEST(MIStoreTests, TestLoadDelete) {
    initLib();

    CatalogObjectH mistore = createMIStore("test_mistore");
    ASSERT_NE(mistore, nullptr);

    // Load tab, shape
    CatalogObjectH shape = getLocalFile("/data/bld.shp");
    ASSERT_NE(shape, nullptr);

    char **options = nullptr;
    options = ngsListAddNameValue(options, "CREATE_OVERVIEWS", "OFF");
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsListAddNameValue(options, "NEW_NAME", "shp_bld");
    options = ngsListAddNameValue(options, "DESCRIPTION", "Длинное русское имя 1");

    EXPECT_EQ(ngsCatalogObjectCopy(shape, mistore, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsListFree(options);
    options = nullptr;
    options = ngsListAddNameValue(options, "CREATE_OVERVIEWS", "OFF");
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsListAddNameValue(options, "NEW_NAME", "tab_bld");
    options = ngsListAddNameValue(options, "DESCRIPTION", "Длинное русское имя 2");

    CatalogObjectH tab = getLocalFile("/data/bld.tab");
    ASSERT_NE(tab, nullptr);

    EXPECT_EQ(ngsCatalogObjectCopy(tab, mistore, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);

    ngsListFree(options);
    options = nullptr;

    ngsCatalogObjectInfo *pathInfo = ngsCatalogObjectQuery(mistore, 0);
    ASSERT_NE(pathInfo, nullptr);
    int count = 0;
    std::string mistorePath = ngsCatalogObjectPath(mistore);
    CatalogObjectH delObject = nullptr;
    while(pathInfo[count].name) {
        delObject = pathInfo[count].object;
        std::cout << count << ". " << mistorePath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 2);
    ngsFree(pathInfo);

    // Delete single layer
    ASSERT_NE(delObject, nullptr);
    EXPECT_EQ(ngsCatalogObjectDelete(delObject), COD_SUCCESS);

    pathInfo = ngsCatalogObjectQuery(mistore, 0);
    ASSERT_NE(pathInfo, nullptr);
    count = 0;
    while(pathInfo[count].name) {
        count++;
    }
    ngsFree(pathInfo);
    EXPECT_GE(count, 1);

    // Delete
    EXPECT_EQ(ngsCatalogObjectDelete(mistore), COD_SUCCESS);

    ngsUnInit();
}

TEST(MIStoreTests, TestLogEdits) {
    initLib();

    CatalogObjectH mistore = createMIStore("test_mistore");
    ASSERT_NE(mistore, nullptr);

     // Load shape
    CatalogObjectH shape = getLocalFile("/data/bld.shp");
    ASSERT_NE(shape, nullptr);

    char **options = nullptr;
    options = ngsListAddNameValue(options, "CREATE_OVERVIEWS", "OFF");
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsListAddNameValue(options, "NEW_NAME", "shp_bld");
    options = ngsListAddNameValue(options, "DESCRIPTION", "Длинное русское имя 1");
    options = ngsListAddNameValue(options, "LOG_EDIT_HISTORY", "ON");

    EXPECT_EQ(ngsCatalogObjectCopy(shape, mistore, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsListFree(options);
    options = nullptr;

    // Edit outside MIStore
    auto basePath = ngsCatalogObjectProperty(mistore, "system_path", "", "");
    auto editPath = ngsFormFileName(basePath, "shp_bld", "tab");
    GDALDataset *DS = static_cast<GDALDataset*>(
                GDALOpenEx(editPath, GDAL_OF_UPDATE|GDAL_OF_SHARED, nullptr,
                           nullptr, nullptr));
    ASSERT_NE(DS, nullptr);
    OGRLayer *layer = DS->GetLayer(0);
    ASSERT_NE(layer, nullptr);

    OGRFeatureDefn *featureDefn = layer->GetLayerDefn();
    OGRFeature *newFeature = OGRFeature::CreateFeature(featureDefn);
    newFeature->SetField("CLCODE", "1");
    newFeature->SetField("CLNAME", "2");
    OGRGeometry* poGeom = nullptr;
    EXPECT_EQ(OGRGeometryFactory::createFromWkt("POLYGON ((0 0 10,0 1 10,1 1 10,1 0 10,0 0 10))",
                                      layer->GetSpatialRef(), &poGeom), OGRERR_NONE);
    EXPECT_EQ(layer->CreateFeature(newFeature), OGRERR_NONE);
    OGRFeature::DestroyFeature(newFeature);

    newFeature = layer->GetFeature(1);
    newFeature->SetField("CLCODE", "3");
    newFeature->SetField("CLNAME", "4");
    EXPECT_EQ(layer->SetFeature(newFeature), OGRERR_NONE);
    OGRFeature::DestroyFeature(newFeature);

    EXPECT_EQ(layer->DeleteFeature(2), OGRERR_NONE);

    GDALClose(DS);

    // Check edits
    CatalogObjectH tab = ngsCatalogObjectGetByName(mistore, "Длинное русское имя 1", 1);
    ASSERT_NE(tab, nullptr);
    ngsEditOperation *ops = ngsFeatureClassGetEditOperations(tab);
    ASSERT_NE(ops, nullptr);
    int count = 0;
    while(ops[count].fid != NOT_FOUND) {
        count++;
    }
    ngsFree(ops);
    EXPECT_GE(count, 2);

    // Delete
    EXPECT_EQ(ngsCatalogObjectDelete(mistore), COD_SUCCESS);

    ngsUnInit();
}

TEST(MIStoreTests, TestLoadFromNGW) {
    initLib();

    // Create connection
    auto connection = createConnection("sandbox.nextgis.com");
    ASSERT_NE(connection, nullptr);

    // NOTE: To use this test the madcity layer must exists.
//    std::string connPath = ngsCatalogObjectPath(connection);
//    std::string testP = connPath + "/examples/madison/madcity";
//    auto testVl = ngsCatalogObjectGet(testP.c_str());
//    auto testId = ngsCatalogObjectProperty(testVl, "id", "", "");
//    EXPECT_STRNE(testId, "");

    // Create resource group
    time_t rawTime = std::time(nullptr);
    auto groupName = "ngstest_group_" + std::to_string(rawTime);
    auto group = createGroup(connection, groupName);
    ASSERT_NE(group, nullptr);

    // Paste local MI tab file with ogr style to NGW vector layer
    resetCounter();
    char **options = nullptr;
    // Add descritpion to NGW vector layer
    options = ngsListAddNameValue(options, "DESCRIPTION", "описание тест1");
    // If source layer has mixed geometries (point + multipoints, lines +
    // multilines, etc.) create output vector layers with multi geometries.
    // Otherwise the layer for each type geometry form source layer will create.
    options = ngsListAddNameValue(options, "FORCE_GEOMETRY_TO_MULTI", "TRUE");
    // Skip empty geometries. Mandatory for NGW?
    options = ngsListAddNameValue(options, "SKIP_EMPTY_GEOMETRY", "TRUE");
    // Check if geometry valid. Non valid geometry will not add to destination
    // layer.
    options = ngsListAddNameValue(options, "SKIP_INVALID_GEOMETRY", "TRUE");
    const char *layerName = "новый слой 4";
    // Set new layer name. If not set, the source layer name will use.
    options = ngsListAddNameValue(options, "NEW_NAME", layerName);
    options = ngsListAddNameValue(options, "OGR_STYLE_TO_FIELD", "TRUE");

    // TODO: Add MI thematic maps (theme Legends) from wor file
//    options = ngsListAddNameValue(options, "WOR_STYLE", "...");

    CatalogObjectH tab = getLocalFile("/data/bld.tab");
    EXPECT_EQ(ngsCatalogObjectCopy(tab, group, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsFree(options);
    EXPECT_GE(getCounter(), 5);

    // Find loaded layer by name
    auto vectorLayer = ngsCatalogObjectGetByName(group, layerName, 1);
    ASSERT_NE(vectorLayer, nullptr);

    EXPECT_STRNE(ngsCatalogObjectProperty(vectorLayer, "id", "", ""), "");

    EXPECT_GE(ngsFeatureClassCount(vectorLayer), 5);

    // Add attachment to first feature
    auto feature = ngsFeatureClassNextFeature(vectorLayer);
    ASSERT_NE(feature, nullptr);

    std::string testPath = ngsGetCurrentDirectory();
    std::string testAttachmentPath = ngsFormFileName(
                testPath.c_str(), "download.cmake", nullptr);
    long long aid = ngsFeatureAttachmentAdd(
                feature, "test.txt", "test add attachment",
                testAttachmentPath.c_str(), nullptr, 0);
    EXPECT_NE(aid, -1);

    // Add MapServer style
    auto style = createStyle(vectorLayer, "новый стиль mapserver",
                             "test Mapserver style", CAT_NGW_MAPSERVER_STYLE,
                             "<map><layer><styleitem>OGR_STYLE</styleitem><class><name>default</name></class></layer></map>");
    ASSERT_NE(style, nullptr);

    // Create MI Store
    CatalogObjectH mistore = createMIStore("test_mistore");
    ASSERT_NE(mistore, nullptr);

    // Paste vector layer to store
    options = nullptr;
    options = ngsListAddNameValue(options, "CREATE_OVERVIEWS", "OFF");
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "OFF");
    // MapInfo has limits to 31 characters for tab file name
    const char *storeLayerName = "t_bld";
    // MapInfo has limits to 255 characters for tab description field (no limits,
    // but in MapINFO GUI will trancate)
    const char *longStoreLayerName = "Длинное русское имя 1";
    options = ngsListAddNameValue(options, "NEW_NAME", storeLayerName);
    options = ngsListAddNameValue(options, "DESCRIPTION", longStoreLayerName);
    options = ngsListAddNameValue(options, "OGR_STYLE_FIELD_TO_STRING", "TRUE");
    options = ngsListAddNameValue(options, "SYNC", "BIDIRECTIONAL");
    options = ngsListAddNameValue(options, "SYNC_ATTACHMENTS", "UPLOAD");
    // Max attachment size for download. Defaults 0 (no download).
    options = ngsListAddNameValue(options, "ATTACHMENTS_DOWNLOAD_MAX_SIZE", "3000");

    resetCounter();
    EXPECT_EQ(ngsCatalogObjectCopy(vectorLayer, mistore, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsFree(options);
    options = nullptr;
    EXPECT_GE(getCounter(), 5);

    // Find loaded layer by name
    auto storeLayer = ngsCatalogObjectGetByName(mistore, longStoreLayerName, 1);
    ASSERT_NE(storeLayer, nullptr);

    EXPECT_GE(ngsFeatureClassCount(storeLayer), 5);
    auto systemPath = ngsCatalogObjectProperty(storeLayer, "system_path", "", "");
    EXPECT_STRNE(systemPath, "");

    // Test overwrite
    options = ngsListAddNameValue(options, "CREATE_OVERVIEWS", "OFF");
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "OFF");
    options = ngsListAddNameValue(options, "OVERWRITE", "ON");
    options = ngsListAddNameValue(options, "NEW_NAME", storeLayerName);
    options = ngsListAddNameValue(options, "DESCRIPTION", longStoreLayerName);
    options = ngsListAddNameValue(options, "OGR_STYLE_FIELD_TO_STRING", "TRUE");
    options = ngsListAddNameValue(options, "SYNC", "BIDIRECTIONAL");
    options = ngsListAddNameValue(options, "SYNC_ATTACHMENTS", "UPLOAD");
    // Max attachment size for download. Defaults 0 (no download).
    options = ngsListAddNameValue(options, "ATTACHMENTS_DOWNLOAD_MAX_SIZE", "3000");

    resetCounter();
    EXPECT_EQ(ngsCatalogObjectCopy(vectorLayer, mistore, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsFree(options);
    EXPECT_GE(getCounter(), 5);

    // Find loaded layer by name
    storeLayer = ngsCatalogObjectGetByName(mistore, longStoreLayerName, 1);
    ASSERT_NE(storeLayer, nullptr);

    // TODO: Modify storeLayer
    EXPECT_EQ(ngsCatalogObjectSync(mistore), 1);

    // Delete resource group
    EXPECT_EQ(ngsCatalogObjectDelete(group), COD_SUCCESS);

    // Delete connection
    EXPECT_EQ(ngsCatalogObjectDelete(connection), COD_SUCCESS);

    // Delete store
    EXPECT_EQ(ngsCatalogObjectDelete(mistore), COD_SUCCESS);

    ngsUnInit();
}

/*
TEST(MIStoreTests, TestLoadFromNGW2) {
    initLib();

    // Create connection
    auto connection = createConnection("mikhail.nextgis.com", "http://");
    ASSERT_NE(connection, nullptr);

    std::string connPath = ngsCatalogObjectPath(connection);
    std::string testP = connPath + "/Красногорск/Линии связи";
    auto vectorLayer = ngsCatalogObjectGet(testP.c_str());
    auto testId = ngsCatalogObjectProperty(vectorLayer, "id", "", "");
    EXPECT_STRNE(testId, "");

    char **options = nullptr;

    // Create MI Store
    CatalogObjectH mistore = createMIStore("test_mistore");
    ASSERT_NE(mistore, nullptr);

    // Paste vector layer to store
    options = ngsListAddNameValue(options, "CREATE_OVERVIEWS", "OFF");
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    // MapInfo has limits to 31 characters for tab file name
    const char *storeLayerName = testId;
    // MapInfo has limits to 255 characters for tab description field (no limits,
    // but in MapINFO GUI will trancate)
    const char *longStoreLayerName = "Длинное русское имя 1";
    options = ngsListAddNameValue(options, "NEW_NAME", storeLayerName);
    options = ngsListAddNameValue(options, "DESCRIPTION", longStoreLayerName);
    options = ngsListAddNameValue(options, "OGR_STYLE_FIELD_TO_STRING", "TRUE");
    options = ngsListAddNameValue(options, "SYNC", "BIDIRECTIONAL");
    options = ngsListAddNameValue(options, "SYNC_ATTACHMENTS", "UPLOAD");
    // Max attachment size for download. Defaults 0 (no download).
    options = ngsListAddNameValue(options, "ATTACHMENTS_DOWNLOAD_MAX_SIZE", "3000");



    resetCounter();
    EXPECT_EQ(ngsCatalogObjectCopy(vectorLayer, mistore, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsFree(options);
    EXPECT_GE(getCounter(), 5);

    // Find loaded layer by name
    auto storeLayer = ngsCatalogObjectGetByName(mistore, longStoreLayerName, 1);
    ASSERT_NE(storeLayer, nullptr);

    EXPECT_GE(ngsFeatureClassCount(storeLayer), 5);
    auto systemPath = ngsCatalogObjectProperty(storeLayer, "system_path", "", "");
    EXPECT_STRNE(systemPath, "");


    // TODO: Modify storeLayer
    EXPECT_EQ(ngsCatalogObjectSync(mistore), 1);

    // Delete connection
    EXPECT_EQ(ngsCatalogObjectDelete(connection), COD_SUCCESS);

    // Delete store
    EXPECT_EQ(ngsCatalogObjectDelete(mistore), COD_SUCCESS);

    ngsUnInit();
}
*/
/*
TEST(StoreTests, TestCreate) {
    EXPECT_EQ(ngsInit(nullptr, nullptr), ngsErrorCodes::EC_SUCCESS);
    const char* epsgPath = CPLFindFile ("gdal", "gcs.csv");
    EXPECT_NE(epsgPath, nullptr);

    OGRSpatialReference wgs;
    wgs.importFromEPSG (4326);
    .SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    OGRSpatialReference wm;
    wm.importFromEPSG (3857);
    .SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    EXPECT_EQ (wgs.IsSame (&wm), 0);

    ngs::DataStorePtr storage = ngs::DataStore::create("./tmp/ngs.gpkg");
    EXPECT_NE(storage, nullptr);
}

TEST(StoreTests, TestOpen) {
    ngs::DataStorePtr storage = ngs::DataStore::open("./tmp/ngs.gpkg");
    EXPECT_NE(storage, nullptr);
}

TEST(StoreTests, TestCreateTMS){
    ngs::DataStorePtr storage = ngs::DataStore::open("./tmp/ngs.gpkg");
    EXPECT_NE(storage, nullptr);
    EXPECT_EQ(storage->datasetCount (), 0);
    counter = 0;
    storage->setNotifyFunc (ngsTestNotifyFunc);
    EXPECT_EQ(storage->createRemoteTMSRaster (TMS_URL, TMS_NAME, TMS_ALIAS,
                                             TMS_COPYING, TMS_EPSG, TMS_MIN_Z,
                                             TMS_MAX_Z, TMS_YORIG_TOP),
              ngsErrorCodes::SUCCESS);
    // rasterCount EXPECT_EQ(storage->datasetCount (), 1);
    EXPECT_GE(counter, 1);
}

TEST(StoreTests, TestDeleteTMS){
    ngs::DataStorePtr storage = ngs::DataStore::open("./tmp/ngs.gpkg");
    EXPECT_NE(storage, nullptr);
    storage->setNotifyFunc (ngsTestNotifyFunc);
    counter = 0;
    EXPECT_EQ(storage.datasetCount (), 1);
    ngs::DatasetPtr pDataset = storage.getDataset (TMS_NAME).lock ();
    ASSERT_NE(pDataset, nullptr);
    EXPECT_EQ(pDataset->destroy (), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(storage.datasetCount (), 0);
    EXPECT_GE(counter, 1);
}

TEST(StoreTests, TestLoad) {
    ngs::DataStorePtr storage = ngs::DataStore::open("./tmp/ngs.gpkg");
    ASSERT_NE(storage, nullptr);

    ngs::DatasetPtr shapeF = ngs::Dataset::open ("./data/bld.shp", GDAL_OF_SHARED|GDAL_OF_READONLY, nullptr);
    ngs::FeatureDataset* const srcTable = ngsDynamicCast(ngs::FeatureDataset, shapeF);
    const OGRSpatialReference *spaRef = srcTable->getSpatialReference ();
    ASSERT_NE(spaRef, nullptr);
    EXPECT_EQ(storage->loadDataset ("test", "./data/bld.shp", "", nullptr,
                                    ngsTestProgressFunc, nullptr), 1);
    CPLSleep(0.6);
    EXPECT_GE(storage->datasetCount (), 0);
    EXPECT_GE(counter, 1);
    ngs::DatasetPtr data = storage->getDataset (0);
    ASSERT_NE(data, nullptr);
    bool isFeatureSet = data->type () & ngsDatasetType(Featureset);
    EXPECT_EQ(isFeatureSet, true);
    ngs::FeatureDataset *fData = dynamic_cast<ngs::FeatureDataset*>(data.get ());
    EXPECT_GE(fData->featureCount(), 3);
}

TEST(StoreTests, TestDelete) {
    ngs::DataStorePtr storage = ngs::DataStore::open("./tmp/ngs.gpkg");
    ASSERT_NE(storage, nullptr);
    EXPECT_EQ(storage->destroy (), ngsErrorCodes::EC_SUCCESS);
    EXPECT_EQ(CSLCount(CPLReadDir("./tmp")), 2); // . and ..
    ngsUninit();
}
*/
