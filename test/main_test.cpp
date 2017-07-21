/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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

static int counter = 0;

void ngsTestNotifyFunc(const char* /*uri*/, enum ngsChangeCode /*operation*/) {
    counter++;
}

int ngsTestProgressFunc(enum ngsCode /*status*/,
                        double /*complete*/, const char* /*message*/,
                        void* /*progressArguments*/) {
    counter++;
    return 1;
}

TEST(BasicTests, TestVersions) {
    EXPECT_EQ(NGS_VERSION_NUM, ngsGetVersion(nullptr));
    EXPECT_STREQ(NGS_VERSION, ngsGetVersionString(nullptr));

    /*EXPECT_EQ(2010000, ngsGetVersion("gdal"));
    EXPECT_STREQ("2.1.0", ngsGetVersionString("gdal"));

    EXPECT_EQ(471040, ngsGetVersion("curl"));
    EXPECT_STREQ("7.48.0", ngsGetVersionString("curl"));

    EXPECT_EQ(10, ngsGetVersion("geos"));
    EXPECT_STREQ("3.5.0-CAPI-1.9.0", ngsGetVersionString("geos"));

    EXPECT_EQ(3011001, ngsGetVersion("sqlite"));
    EXPECT_STREQ("3.11.1", ngsGetVersionString("sqlite"));    

    EXPECT_EQ(3171, ngsGetVersion("jsonc"));
    EXPECT_STREQ("0.12.99", ngsGetVersionString("jsonc"));

    EXPECT_EQ(492, ngsGetVersion("proj"));
    EXPECT_STREQ("4.9.2", ngsGetVersionString("proj"));

    EXPECT_EQ(90, ngsGetVersion("jpeg"));
    EXPECT_STREQ("9.1", ngsGetVersionString("jpeg"));

    EXPECT_EQ(43, ngsGetVersion("tiff"));
    EXPECT_STREQ("4.3", ngsGetVersionString("tiff"));

    EXPECT_EQ(1410, ngsGetVersion("geotiff"));
    EXPECT_STREQ("1.4.1", ngsGetVersionString("geotiff"));

    EXPECT_EQ(10621, ngsGetVersion("png"));
    EXPECT_STREQ("1.6.21", ngsGetVersionString("png"));

    EXPECT_EQ(210, ngsGetVersion("expat"));
    EXPECT_STREQ("2.1.0", ngsGetVersionString("expat"));

    EXPECT_EQ(270, ngsGetVersion("iconv"));
    EXPECT_STREQ("1.14", ngsGetVersionString("iconv"));

    EXPECT_EQ(4736, ngsGetVersion("zlib"));
    EXPECT_STREQ("1.2.8", ngsGetVersionString("zlib"));

    EXPECT_EQ(268443663, ngsGetVersion("openssl"));
    EXPECT_STREQ("1.0.2", ngsGetVersionString("openssl"));*/

    char** options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);

    ngsDestroyList(options);


    const char* formats = ngsGetVersionString("formats");
    EXPECT_NE(nullptr, formats);
    std::cout << "Available formats:\n" << formats << std::endl;

    ngsUnInit();
}

TEST(BasicTests, TestInlines) {
    ngsRGBA color = {254, 253, 252, 251};
    int hexColor = ngsRGBA2HEX(color);
    ngsRGBA newColor = ngsHEX2RGBA(hexColor);
    EXPECT_EQ(color.R, newColor.R);
    EXPECT_EQ(color.G, newColor.G);
    EXPECT_EQ(color.B, newColor.B);
    EXPECT_EQ(color.A, newColor.A);
}

TEST(CatalogTests, TestCatalogQuery) {
    char** options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);

    ngsDestroyList(options);
    CatalogObjectH catalog = ngsCatalogObjectGet("ngc://");
    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        count++;
    }
    ASSERT_GE(count, 1);
    CPLString path2test = CPLSPrintf("ngc://%s", pathInfo[0].name);
    ngsFree(pathInfo);

    CatalogObjectH path2testObject = ngsCatalogObjectGet(path2test);
    pathInfo = ngsCatalogObjectQuery(path2testObject, 0);
    ASSERT_NE(pathInfo, nullptr);
    count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << path2test << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 1);
    path2test = CPLSPrintf("%s/%s", path2test.c_str(), pathInfo[0].name);
    ngsFree(pathInfo);

    path2testObject = ngsCatalogObjectGet(path2test);
    pathInfo = ngsCatalogObjectQuery(path2testObject, 0);
    ASSERT_NE(pathInfo, nullptr);
    count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << path2test << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 1);
    ngsFree(pathInfo);

    // Test zip support
    CPLString catalogPath = ngsCatalogPathFromSystem(CPLGetCurrentDir());
    CPLString zipPath = catalogPath + "/data/railway.zip";
    CatalogObjectH zipObject = ngsCatalogObjectGet(zipPath);
    pathInfo = ngsCatalogObjectQuery(zipObject, 0);
    count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << zipPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 1);
    ngsFree(pathInfo);

    ngsUnInit();
}

TEST(CatalogTests, TestCreate) {
    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    // TODO: CACHE_DIR, GDAL_DATA, LOCALE
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);
    ngsDestroyList(options);
    options = nullptr;

    CPLString path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr);
    CPLString catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");

    options = ngsAddNameValue(options, "TYPE",
                              std::to_string(CAT_CONTAINER_DIR).c_str());
    options = ngsAddNameValue(options, "CREATE_UNIQUE", "ON");

    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath);
    EXPECT_EQ(ngsCatalogObjectCreate(catalog, "test_dir1", options),
              ngsCode::COD_SUCCESS);
    EXPECT_EQ(ngsCatalogObjectCreate(catalog, "test_dir1", options),
              ngsCode::COD_SUCCESS);
    ngsDestroyList(options);
    options = nullptr;

    ngsCatalogObjectInfo *pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 2);
    ngsFree(pathInfo);

    options = ngsAddNameValue(options, "TYPE",
                              std::to_string(CAT_RASTER_TMS).c_str());
    options = ngsAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsAddNameValue(options, "url", "http://tile.openstreetmap.org/{z}/{x}/{y}.png");
    options = ngsAddNameValue(options, "epsg", "3857");
    options = ngsAddNameValue(options, "z_min", "0");
    options = ngsAddNameValue(options, "z_max", "19");

    EXPECT_EQ(ngsCatalogObjectCreate(catalog, "osm.wconn", options),
              ngsCode::COD_SUCCESS);

    pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ASSERT_NE(pathInfo, nullptr);
    count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 3);
    ngsFree(pathInfo);

    ngsUnInit();
}

TEST(CatalogTests, TestDelete) {
    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);
    ngsDestroyList(options);

    CPLString path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr);
    CPLString catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");
    CPLString delPath = ngsFormFileName(catalogPath, "test_dir1", nullptr);
    CatalogObjectH delObject = ngsCatalogObjectGet(delPath);
    EXPECT_EQ(ngsCatalogObjectDelete(delObject), ngsCode::COD_SUCCESS);
    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath);
    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 2);
    ngsFree(pathInfo);
    ngsUnInit();
}

TEST(DataStoreTests, TestCreateDataStore) {
    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);
    ngsDestroyList(options);
    options = nullptr;

    CPLString path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr);
    CPLString catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");

    options = ngsAddNameValue(options, "TYPE",
                              std::to_string(CAT_CONTAINER_NGS).c_str());
    options = ngsAddNameValue(options, "CREATE_UNIQUE", "ON");
    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath);
    EXPECT_EQ(ngsCatalogObjectCreate(catalog, "main", options),
              ngsCode::COD_SUCCESS);
    ngsCatalogObjectInfo *pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 3);
    ngsFree(pathInfo);
    ngsUnInit();
}

TEST(DataStoreTests, TestOpenDataStore) {
    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);

    ngsDestroyList(options);
    CPLString path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr);
    CPLString catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");
    CPLString storePath = ngsFormFileName(catalogPath, "main", "ngst");
    CatalogObjectH store = ngsCatalogObjectGet(storePath);
    ngsCatalogObjectInfo *pathInfo = ngsCatalogObjectQuery(store, 0);

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
    counter = 0;

    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);

    ngsDestroyList(options);

    CPLString testPath = ngsGetCurrentDirectory();
    CPLString catalogPath = ngsCatalogPathFromSystem(testPath);
    CPLString storePath = catalogPath + "/tmp/main.ngst";
    CPLString shapePath = catalogPath + "/data/bld.shp";
    CatalogObjectH store = ngsCatalogObjectGet(storePath);
    CatalogObjectH shape = ngsCatalogObjectGet(shapePath);

    options = nullptr;
    options = ngsAddNameValue(options, "CREATE_OVERVIEWS", "ON");

    EXPECT_EQ(ngsCatalogObjectLoad(shape, store, options,
                                   ngsTestProgressFunc, nullptr),
              ngsCode::COD_SUCCESS);
    ngsDestroyList(options);
    EXPECT_GE(counter, 1);

    ngsUnInit();
}

TEST(DataStoreTests, TestLoadDataStoreZippedShapefile) {
    counter = 0;

    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);

    ngsDestroyList(options);

    CPLString testPath = ngsGetCurrentDirectory();
    CPLString catalogPath = ngsCatalogPathFromSystem(testPath);
    CPLString storePath = catalogPath + "/tmp/main.ngst";
    CPLString shapePath = catalogPath + "/data/railway.zip/railway-line.shp";
    CatalogObjectH store = ngsCatalogObjectGet(storePath);
    CatalogObjectH shape = ngsCatalogObjectGet(shapePath);
    EXPECT_EQ(ngsCatalogObjectLoad(shape, store, nullptr,
                                   ngsTestProgressFunc, nullptr),
              ngsCode::COD_SUCCESS);
    EXPECT_GE(counter, 1);
    ngsUnInit();
}

TEST(DataStoreTests, TestDeleteDataStore) {
    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);
    ngsDestroyList(options);

    CPLString path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr);
    CPLString catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");
    CPLString delPath = ngsFormFileName(catalogPath, "main", "ngst");
    CatalogObjectH delObject = ngsCatalogObjectGet(delPath);
    EXPECT_EQ(ngsCatalogObjectDelete(delObject), ngsCode::COD_SUCCESS);
    ngsUnInit();
}

constexpr ngsRGBA DEFAULT_MAP_BK = {199, 199, 199, 199};
constexpr const char* DEFAULT_MAP_NAME = "new map";

TEST(MapTests, MapSave) {
    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);
    ngsDestroyList(options);

    CPLString testPath = ngsGetCurrentDirectory();
    CPLString catalogPath = ngsCatalogPathFromSystem(testPath);
    CPLString shapePath = catalogPath + "/data/bld.shp";
    CPLString mapPath = catalogPath + "/tmp/test_map.ngmd";

    unsigned char mapId = ngsMapCreate(DEFAULT_MAP_NAME, "", ngs::DEFAULT_EPSG,
                                       ngs::DEFAULT_BOUNDS.minX(),
                                       ngs::DEFAULT_BOUNDS.minY(),
                                       ngs::DEFAULT_BOUNDS.maxX(),
                                       ngs::DEFAULT_BOUNDS.maxY());
    ASSERT_NE(mapId, 0);
    EXPECT_EQ(ngsMapLayerCount(mapId), 0);

    EXPECT_EQ(ngsMapCreateLayer(mapId, "Layer 0", shapePath), 0);

    EXPECT_EQ(ngsMapCreateLayer(mapId, "Layer 1", shapePath), 1);

    EXPECT_EQ(ngsMapLayerCount(mapId), 2);

    EXPECT_EQ(ngsMapSetBackgroundColor(mapId, DEFAULT_MAP_BK),
              ngsCode::COD_SUCCESS);

    EXPECT_EQ(ngsMapSave(mapId, mapPath), ngsCode::COD_SUCCESS);

    ngsUnInit();
}

TEST(MapTests, MapOpen) {
    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);
    ngsDestroyList(options);

    CPLString testPath = ngsGetCurrentDirectory();
    CPLString catalogPath = ngsCatalogPathFromSystem(testPath);
    CPLString mapPath = catalogPath + "/tmp/test_map.ngmd";

    unsigned char mapId = ngsMapOpen(mapPath);
    ASSERT_NE(mapId, 0);

    EXPECT_EQ(ngsMapLayerCount(mapId), 2);

    ngsRGBA bk = ngsMapGetBackgroundColor(mapId);

    EXPECT_EQ(bk.R, DEFAULT_MAP_BK.R);
    EXPECT_EQ(bk.A, DEFAULT_MAP_BK.A);

    LayerH layer0 = ngsMapLayerGet(mapId, 0);
    LayerH layer1 = ngsMapLayerGet(mapId, 1);

    CPLString layer1Name = ngsLayerGetName(layer1);

    EXPECT_EQ(ngsMapLayerReorder(mapId, layer0, layer1),
              ngsCode::COD_SUCCESS);

    LayerH layerTest = ngsMapLayerGet(mapId, 0);
    CPLString layerTestName = ngsLayerGetName(layerTest);
    EXPECT_STREQ(layer1Name, layerTestName);

    EXPECT_EQ(ngsMapLayerDelete(mapId, layerTest),
              ngsCode::COD_SUCCESS);

    EXPECT_EQ(ngsMapLayerCount(mapId), 1);

    ngsUnInit();
}

TEST(MiscTests, TestURLRequest) {
    char **options = nullptr;
    options = ngsAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    options = ngsAddNameValue(options, "SSL_CERT_FILE", "~/tmp/no.pem");
    EXPECT_EQ(ngsInit(options), ngsCode::COD_SUCCESS);
    ngsDestroyList(options);

    options = nullptr;
    options = ngsAddNameValue(options, "MAX_RETRY", "10");

    ngsURLRequestResult *result = ngsURLRequest(ngsURLRequestType::URT_GET,
                                               "http://ya.ru", options);
    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->status, 200);
    EXPECT_LT(result->status, 400);
    ngsURLRequestDestroyResult(result);
    ngsDestroyList(options);

    result = ngsURLRequest(ngsURLRequestType::URT_GET,
            "http://demo.nextgis.com/api/component/pyramid/pkg_version", nullptr);

    EXPECT_GE(result->status, 200);
    EXPECT_LT(result->status, 400);
    CPLString data(reinterpret_cast<const char*>(result->data),
                   static_cast<size_t>(result->dataLen));
    std::cout << data << std::endl;
    ngsURLRequestDestroyResult(result);

    options = nullptr;
    options = ngsAddNameValue(options, "UNSAFESSL", "ON");
    options = ngsAddNameValue(options, "MAX_RETRY", "10");
    result = ngsURLRequest(ngsURLRequestType::URT_GET,
            "https://nextgis.com", options);

    EXPECT_GE(result->status, 200);
    EXPECT_LT(result->status, 400);
    ngsURLRequestDestroyResult(result);
    ngsDestroyList(options);

    options = nullptr;
    options = ngsAddNameValue(options, "MAX_RETRY", "10");
    result = ngsURLRequest(ngsURLRequestType::URT_GET,
            "http://tile.openstreetmap.org/9/309/160.png", options);

    EXPECT_GE(result->status, 200);
    EXPECT_LT(result->status, 400);

//    std::ofstream outFile;
//    outFile.open("osm.png", std::ios::out | std::ios::binary);
//    outFile.write(reinterpret_cast<const char*>(result->data), result->dataLen);

    ngsURLRequestDestroyResult(result);
    ngsDestroyList(options);

    ngsUnInit();
}
