/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2025 NextGIS, <info@nextgis.com>
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

#include <iostream>
#include <fstream>

// gdal
#include "cpl_string.h"

#include "api_priv.h"
#include "ds/geometry.h"
#include "ngstore/api.h"
#include "ngstore/version.h"

static CatalogObjectH createDataStore(const std::string &name, CatalogObjectH catalog) {
    char **options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_CONTAINER_NGS);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    CatalogObjectH out = ngsCatalogObjectCreate(catalog, name.c_str(), options);
    ngsListFree(options);
    return out;
}

static long gpsTime()
{
    return time(nullptr);
}

std::string &getStoreName() {
    static std::string val = "main" + std::to_string(gpsTime());
    return val;
}

TEST(DataStoreTests, TestCreateDataStore) {
    initLib();
    char **options = nullptr;

    std::string path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr, 0);
    std::string catalogPath = ngsCatalogPathFromSystem(path.c_str());
    ASSERT_STRNE(catalogPath.c_str(), "");

    options = ngsListAddNameIntValue(options, "TYPE", CAT_CONTAINER_NGS);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath.c_str());
    CatalogObjectH store = createDataStore(getStoreName(), catalog);
    EXPECT_NE(store, nullptr);
    ngsCatalogObjectInfo *pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ngsListFree(options);
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 2); // At least connections and storage
    ngsFree(pathInfo);
    ngsUnInit();
}

TEST(DataStoreTests, TestOpenDataStore) {
    initLib();
    std::string path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr, 0);
    std::string catalogPath = ngsCatalogPathFromSystem(path.c_str());
    ASSERT_STRNE(catalogPath.c_str(), "");
    std::string storePath = ngsFormFileName(catalogPath.c_str(), getStoreName().c_str(), "ngst", 1);
    CatalogObjectH store = ngsCatalogObjectGet(storePath.c_str());
    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(store, 0);

    size_t count = 0;

    if(nullptr != pathInfo) {
        while(pathInfo[count].name) {
            count++;
        }
    }
    ASSERT_GE(count, 0);
    ngsFree(pathInfo);

    ngsUnInit();
}

TEST(DataStoreTests, TestLoadDataStore) {
    initLib();
    resetCounter();

    std::string testPath = ngsGetCurrentDirectory();
    std::string catalogPath = ngsCatalogPathFromSystem(testPath.c_str());
    std::string storePath = catalogPath + "/tmp/" + getStoreName() + ".ngst";
    std::string shapePath = catalogPath + "/data/bld.shp";
    CatalogObjectH store = ngsCatalogObjectGet(storePath.c_str());
    CatalogObjectH shape = ngsCatalogObjectGet(shapePath.c_str());

    char **options = nullptr;
    options = ngsListAddNameValue(options, "CREATE_OVERVIEWS", "ON");

    EXPECT_EQ(ngsCatalogObjectCopy(shape, store, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsListFree(options);
    EXPECT_GE(getCounter(), 1);

    ngsUnInit();
}

TEST(DataStoreTests, TestLoadDataStoreZippedShapefile) {
    initLib();
    resetCounter();

    std::string testPath = ngsGetCurrentDirectory();
	std::string catalogPath = ngsCatalogPathFromSystem(testPath.c_str());
	std::string storePath = catalogPath + "/tmp/" + getStoreName() + ".ngst";
	std::string shapePath = catalogPath + "/data/railway.zip/railway-line.shp";
    CatalogObjectH store = ngsCatalogObjectGet(storePath.c_str());
    CatalogObjectH shape = ngsCatalogObjectGet(shapePath.c_str());

    ngsFeatureClassBatchMode(store, 1);
    EXPECT_EQ(ngsCatalogObjectCopy(shape, store, nullptr,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    EXPECT_EQ(ngsCatalogObjectCopy(shape, store, nullptr,
                                   ngsTestProgressFunc, nullptr), 
                                   COD_FUNCTION_NOT_AVAILABLE);
    shapePath = catalogPath + "/data/railway-mini.zip/railway-mini.shp";
    shape = ngsCatalogObjectGet(shapePath.c_str());
    EXPECT_EQ(ngsCatalogObjectCopy(shape, store, nullptr,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsFeatureClassBatchMode(store, 0);

    EXPECT_GE(getCounter(), 1);
    ngsUnInit();
}

TEST(DataStoreTests, TestLoadAndDelete) {
    initLib();
    resetCounter();

    CPLString testPath = ngsGetCurrentDirectory();
    CPLString catalogPath = ngsCatalogPathFromSystem(testPath);
    CPLString storePath = catalogPath + "/tmp/" + getStoreName() + ".ngst";
    CPLString shapePath = catalogPath + "/data/bld.shp";
    CatalogObjectH store = ngsCatalogObjectGet(storePath);
    CatalogObjectH shape = ngsCatalogObjectGet(shapePath);

    char **options = nullptr;
    options = ngsListAddNameValue(options, "NEW_NAME", "delete_me");

    EXPECT_EQ(ngsCatalogObjectCopy(shape, store, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsListFree(options);
    EXPECT_GE(getCounter(), 1);

    CatalogObjectH newFC1 = ngsCatalogObjectGet(CPLString(storePath + "/delete_me"));
    EXPECT_NE(newFC1, nullptr);

    EXPECT_GE(ngsFeatureClassCount(newFC1), 1);

    EXPECT_EQ(ngsCatalogObjectDelete(newFC1), COD_SUCCESS);

    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(store, 0);
    int count = 0;
    while(pathInfo[count].name != nullptr) {
        std::cout << "Path:" << storePath << "/" << pathInfo[count].name << "\n";
        count++;
    }

    ngsFree(pathInfo);

    ngsUnInit();
}

TEST(DataStoreTests, TestCopyFCToGeoJSON) {
    initLib();
    resetCounter();

    CPLString testPath = ngsGetCurrentDirectory();
    CPLString catalogPath = ngsCatalogPathFromSystem(testPath);
    CPLString storePath = catalogPath + "/tmp/" + getStoreName() + ".ngst/bld";
    CPLString jsonPath = catalogPath + "/tmp";

    CatalogObjectH store = ngsCatalogObjectGet(storePath);
    CatalogObjectH json = ngsCatalogObjectGet(jsonPath);

    char** options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_FC_GEOJSON);
    options = ngsListAddNameValue(options, "OVERWRITE", "ON");
    EXPECT_EQ(ngsCatalogObjectCopy(store, json, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);

    ngsListFree(options);
    EXPECT_GE(getCounter(), 1);
    ngsUnInit();
}

TEST(DataStoreTests, TestCreateFeatureClass) {
    initLib();

    std::string testPath = ngsGetCurrentDirectory();
    std::string catalogPath = ngsCatalogPathFromSystem(testPath.c_str());
    std::string storePath = catalogPath + "/tmp/" + getStoreName() + ".ngst";
    CatalogObjectH store = ngsCatalogObjectGet(storePath.c_str());
    ASSERT_NE(store, nullptr);
    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(store, 0);
    ngsFree(pathInfo);

    char** options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_FC_GPKG);
    options = ngsListAddNameValue(options, "USER.SOURCE_URL", NEXTGIS_URL);
    options = ngsListAddNameValue(options, "USER.SOURCE_SRS", "4326");
    options = ngsListAddNameValue(options, "GEOMETRY_TYPE", "POINT");
    options = ngsListAddNameValue(options, "FIELD_COUNT", "4");
    options = ngsListAddNameValue(options, "FIELD_0_TYPE", "INTEGER");
    options = ngsListAddNameValue(options, "FIELD_0_NAME", "type");
    options = ngsListAddNameValue(options, "FIELD_0_ALIAS", "тип");
    options = ngsListAddNameValue(options, "FIELD_1_TYPE", "STRING");
    options = ngsListAddNameValue(options, "FIELD_1_NAME", "desc");
    options = ngsListAddNameValue(options, "FIELD_1_ALIAS", "описание");
    options = ngsListAddNameValue(options, "FIELD_2_TYPE", "REAL");
    options = ngsListAddNameValue(options, "FIELD_2_NAME", "val");
    options = ngsListAddNameValue(options, "FIELD_2_ALIAS", "плавающая точка");
    options = ngsListAddNameValue(options, "FIELD_3_TYPE", "DATE_TIME");
    options = ngsListAddNameValue(options, "FIELD_3_NAME", "date");
    options = ngsListAddNameValue(options, "FIELD_3_ALIAS", "Это дата");
    options = ngsListAddNameValue(options, "LOG_EDIT_HISTORY", "ON");

    EXPECT_NE(ngsCatalogObjectCreate(store, "new_layer", options), nullptr);
    ngsListFree(options);

    CatalogObjectH newFC = ngsCatalogObjectGet(CPLString(storePath + "/new_layer"));
    EXPECT_NE(newFC, nullptr);

    options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_FC_GPKG);
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
    options = ngsListAddNameValue(options, "CREATE_OVERVIEWS", "ON");
    options = ngsListAddNameValue(options, "ZOOM_LEVELS", "3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18");

    EXPECT_NE(ngsCatalogObjectCreate(store, "r_1502560461_99719", options), nullptr);
    ngsListFree(options);

    CatalogObjectH newFC1 = ngsCatalogObjectGet(CPLString(storePath + "/r_1502560461_99719"));
    EXPECT_NE(newFC1, nullptr);

    ngsUnInit();
}

TEST(DataStoreTests, TestCreateFeature) {
    initLib();

	std::string testPath = ngsGetCurrentDirectory();
    std::string catalogPath = ngsCatalogPathFromSystem(testPath.c_str());
    std::string fcPath = catalogPath + "/tmp/" + getStoreName() + ".ngst/new_layer";
    CatalogObjectH featureClass = ngsCatalogObjectGet(fcPath.c_str());

    ASSERT_NE(featureClass, nullptr);
    EXPECT_EQ(ngsFeatureClassCount(featureClass), 0);

    ngsField *fields = ngsFeatureClassFields(featureClass);
    ASSERT_NE(fields, nullptr);
    int count = 0;
    while(fields[count].name != nullptr) {
        EXPECT_NE(fields[count].name, "");
        std::cout << "Name: " << fields[count].name << " Alias: " << fields[count].alias << "\n";
        count++;
    }
    ngsFree(fields);

    EXPECT_GE(count, 1);

    FeatureH newFeature = ngsFeatureClassCreateFeature(featureClass);
    ASSERT_NE(newFeature, nullptr);
    GeometryH geom = ngsFeatureCreateGeometry(newFeature);
    ASSERT_NE(geom, nullptr);

    ngsGeometrySetPoint(geom, 0, 37.5, 55.1, 0.0, 0.0);
    ngsFeatureSetGeometry(newFeature, geom); // NOTE: Node need to free geometry as we attach it to feature
    ngsFeatureSetFieldInteger(newFeature, 0, 500);
    ngsFeatureSetFieldString(newFeature, 1, "Test");
    ngsFeatureSetFieldDateTime(newFeature, 3, 1961, 4, 12, 6, 7, 0, 0);

    ngsFeatureClassBatchMode(featureClass, 1);
    EXPECT_EQ(ngsFeatureClassInsertFeature(featureClass, newFeature, 1), COD_SUCCESS);
    ngsFeatureClassBatchMode(featureClass, 0);

    auto fid = ngsFeatureGetId(newFeature);
    EXPECT_NE(fid, -1);

    ngsFeatureFree(newFeature);

    EXPECT_EQ(ngsFeatureClassCount(featureClass), 1);
    newFeature = ngsFeatureClassGetFeature(featureClass, fid);
    ASSERT_NE(newFeature, nullptr);

    EXPECT_EQ(ngsFeatureIsFieldSet(newFeature, 2), 0);
    EXPECT_EQ(ngsFeatureGetFieldAsInteger(newFeature, 0), 500);
    ngsFeatureSetFieldDouble(newFeature, 2, 555.777);

    EXPECT_EQ(ngsFeatureClassUpdateFeature(featureClass, newFeature, 1), COD_SUCCESS);

    std::string testAttachmentPath = ngsFormFileName(testPath.c_str(), "download.cmake",
														nullptr, 0);
    long long id = ngsFeatureAttachmentAdd(newFeature, "test.txt",
                                           "test add atachment",
                                           testAttachmentPath.c_str(), nullptr, 1);
    EXPECT_NE(id, -1);

    VSIStatBufL sbuf;
    std::string path1 = testPath + "/tmp/" + getStoreName() + ".attachments";
    EXPECT_EQ(VSIStatL(path1.c_str(), &sbuf), 0);

    std::string path2 = path1 + "/new_layer";
    EXPECT_EQ(VSIStatL(path2.c_str(), &sbuf), 0);

    std::string path3 = path2 + "/1";
    EXPECT_EQ(VSIStatL(path3.c_str(), &sbuf), 0);

    ngsFeatureAttachmentInfo *list = ngsFeatureAttachmentsGet(newFeature);
    ASSERT_NE(list, nullptr);
    int counter = 0;
    while (list[counter].id != -1) {
        std::cout << "Attach -- name: " << list[counter].name <<
                     " | description: " << list[counter].description <<
                     "\n     path: " << list[counter].path << " | size: " <<
                     list[counter].size <<
                     "\n id: " << list[counter].id << "\n";
        counter++;
    }
    ASSERT_GE(counter, 1);
    ngsFree(list);

    list = ngsFeatureAttachmentsGet(newFeature);

    ngsFeatureFree(newFeature);
    ASSERT_NE(list, nullptr);
    counter = 0;
    while (list[counter].id != -1) {
        counter++;
    }
    ASSERT_GE(counter, 1);
    ngsFree(list);

    ngsFeatureChange *ops = ngsFeatureClassGetEditOperations(featureClass);
    ASSERT_NE(ops, nullptr);
    counter = 0;
    bool hasCreate = false;
    while(ops[counter].fid != -1) {
        std::cout << "Edit operation: fid - " << ops[counter].fid <<
                     " aid - " << ops[counter].aid <<
                     " code - " << ops[counter].code << "\n";
        if(!hasCreate) {
            hasCreate = ops[counter].code == CC_CREATE_FEATURE;
        }
        counter++;
    }
    EXPECT_GE(counter, 2);
    EXPECT_EQ(hasCreate, true); // Check that insert and update operations log only insert
    ngsFree(ops);

    ngsUnInit();
}

TEST(DataStoreTest, TestTracksTable) {
	initLib();

    std::string testPath = ngsGetCurrentDirectory();
    std::string catalogPath = ngsCatalogPathFromSystem(testPath.c_str());
    catalogPath += "/tmp/";
    std::string storePath = catalogPath + getStoreName() + ".ngst";
    CatalogObjectH store = ngsCatalogObjectGet(storePath.c_str());
	char **options = nullptr;
    if(store == nullptr) {
        options = nullptr;
        options = ngsListAddNameIntValue(options, "TYPE", CAT_CONTAINER_NGS);
        options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
        CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath.c_str());
        EXPECT_NE(ngsCatalogObjectCreate(catalog, getStoreName().c_str(), options), nullptr);
        ngsListFree(options);
    }

    options = nullptr;
    store = ngsCatalogObjectGet(storePath.c_str());
    ASSERT_NE(store, nullptr);
    CatalogObjectH tracks = ngsStoreGetTracksTable(store);
    ASSERT_NE(tracks, nullptr);

    EXPECT_EQ(ngsStoreHasTracksTable(store), 1);

    EXPECT_EQ(ngsTrackAddPoint(tracks, "test1", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, gpsTime(), 0, 1, 0), 1);
    CPLSleep(1.5);
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test1", 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, gpsTime(), 1, 0, 0), 1);
    CPLSleep(1.5);
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test1", 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, gpsTime(), 2, 0, 0), 1);
    CPLSleep(1.5);
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test1", 3.0, 3.0, 3.0, 3.0, 3.0, 3.0, gpsTime(), 3, 0, 0), 1);
    CPLSleep(1.5);
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test1", 4.0, 4.0, 4.0, 4.0, 4.0, 4.0, gpsTime(), 4, 0, 0), 1);
    CPLSleep(2.0);
    // New segment
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test1", 5.0, 5.0, 5.0, 5.0, 5.0, 5.0, gpsTime(), 5, 0, 1), 1);
    CPLSleep(1.5);
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test1", 6.0, 6.0, 6.0, 6.0, 6.0, 6.0, gpsTime(), 6, 0, 0), 1);
    CPLSleep(1.5);
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test1", 7.0, 7.0, 7.0, 7.0, 7.0, 7.0, gpsTime(), 7, 0, 0), 1);
    CPLSleep(2.0);
    // New track
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test2", 8.0, 8.0, 8.0, 8.0, 8.0, 8.0, gpsTime(), 8, 1, 0), 1);
    CPLSleep(1.5);
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test2", 9.0, 9.0, 9.0, 9.0, 9.0, 9.0, gpsTime(), 9, 0, 0), 1);
    CPLSleep(1.5);
    EXPECT_EQ(ngsTrackAddPoint(tracks, "test2", 10.0, 10.0, 10.0, 10.0, 10.0, 10.0, gpsTime(), 10, 0, 0), 1);

    int counter = 0;
    ngsTrackInfo *info = ngsTrackGetList(tracks);
    time_t start(0), stop(0);
    while(info[counter].name != nullptr) {
        std::cout << counter << ". " << info[counter].name << ", "
                  << info[counter].startTimeStamp << " -- "
                  << info[counter].stopTimeStamp << "\n";
        start = static_cast<time_t>(info[counter].startTimeStamp);
        stop = static_cast<time_t>(info[counter].stopTimeStamp);
        counter++;
    }
    ngsFree(info);
    ASSERT_GE(counter, 2);

    char buffer [80];
    strftime (buffer,80,"%FT%TZ", localtime(&start));
    std::string startStr = buffer;
    strftime (buffer,80,"%FT%TZ", localtime(&stop));
    std::string stopStr = buffer;

    CatalogObjectH tracksPoints = ngsTrackGetPointsTable(tracks);
    EXPECT_EQ(ngsFeatureClassSetFilter(tracksPoints, nullptr,
                                       CPLSPrintf("time_stamp >= '%s' and time_stamp <= '%s'",
                                                  startStr.c_str(), stopStr.c_str())),
              COD_SUCCESS);

    // Export to GPX
    options = ngsListAddNameIntValue(options, "TYPE", CAT_FC_GPX);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsListAddNameValue(options, "NEW_NAME", "test_tracks");
    options = ngsListAddNameValue(options, "LAYER_NAME", "track_points");
    options = ngsListAddNameValue(options, "GPX_USE_EXTENSIONS", "ON");
    options = ngsListAddNameValue(options, "SKIP_EMPTY_GEOMETRY", "ON");

    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath.c_str());
    EXPECT_EQ(ngsCatalogObjectCopy(tracksPoints, catalog, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);
    ngsListFree(options);

//    ngsTrackSync(tracks, 5);

    ngsUnInit();
}

TEST(DataStoreTest, TestCreateMemoryDatasource) {
	initLib();
    char **options = nullptr;

    CPLString testPath = ngsGetCurrentDirectory();
    CPLString catalogPath = ngsCatalogPathFromSystem(testPath);
    CPLString storePath = catalogPath + "/tmp";
    CatalogObjectH store = ngsCatalogObjectGet(storePath);

    options = ngsListAddNameIntValue(options, "TYPE", CAT_CONTAINER_MEM);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    EXPECT_NE(ngsCatalogObjectCreate(store, "test_mem", options), nullptr);
    CatalogObjectH newStore = ngsCatalogObjectGet(CPLString(storePath + "/test_mem.ngmem"));
    EXPECT_NE(newStore, nullptr);

    // Create feature class in memory
    ngsListFree(options);
    options = nullptr;

    options = ngsListAddNameIntValue(options, "TYPE", CAT_FC_MEM);
    options = ngsListAddNameValue(options, "EPSG", "3857");
    options = ngsListAddNameValue(options, "LCO.ADVERTIZE_UTF8", "ON");
    options = ngsListAddNameValue(options, "GEOMETRY_TYPE", "POINT");
    options = ngsListAddNameValue(options, "FIELD_COUNT", "4");
    options = ngsListAddNameValue(options, "FIELD_0_TYPE", "INTEGER");
    options = ngsListAddNameValue(options, "FIELD_0_NAME", "type");
    options = ngsListAddNameValue(options, "FIELD_0_ALIAS", "тип");
    options = ngsListAddNameValue(options, "FIELD_1_TYPE", "STRING");
    options = ngsListAddNameValue(options, "FIELD_1_NAME", "desc");
    options = ngsListAddNameValue(options, "FIELD_1_ALIAS", "описание");
    options = ngsListAddNameValue(options, "FIELD_2_TYPE", "REAL");
    options = ngsListAddNameValue(options, "FIELD_2_NAME", "val");
    options = ngsListAddNameValue(options, "FIELD_2_ALIAS", "плавающая точка");
    options = ngsListAddNameValue(options, "FIELD_3_TYPE", "DATE_TIME");
    options = ngsListAddNameValue(options, "FIELD_3_NAME", "date");
    options = ngsListAddNameValue(options, "FIELD_3_ALIAS", "Это дата");

    EXPECT_NE(ngsCatalogObjectCreate(newStore, "new_layer", options), nullptr);
    ngsListFree(options);

    CatalogObjectH newFC = ngsCatalogObjectGet(CPLString(storePath + "/test_mem.ngmem/new_layer"));
    EXPECT_NE(newFC, nullptr);

    ngsUnInit();
}

TEST(DataStoreTests, TestDeleteDataStore) {
	initLib();

    auto path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr, 0);
    auto catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");
    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath);
    CatalogObjectH store = createDataStore(getStoreName() + "_1", catalog);
    EXPECT_NE(store, nullptr);
    ngsCatalogObjectClose(store);
    auto delPath = ngsFormFileName(catalogPath, std::string(getStoreName() + "_1").c_str(), "ngst", 1);
    CatalogObjectH delObject = ngsCatalogObjectGet(delPath);
    EXPECT_EQ(ngsCatalogObjectDelete(delObject), COD_SUCCESS);
    ngsUnInit();
}
