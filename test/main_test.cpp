/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2024 NextGIS, <info@nextgis.com>
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
#include "ngstore/version.h"


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

    initLib();

    const char* formats = ngsGetVersionString("formats");
    EXPECT_NE(nullptr, formats);
    std::cout << "Available formats:\n" << formats << std::endl;

    ngsUnInit();
}

TEST(BasicTests, TestInlines) {
    ngsRGBA color = {254, 253, 252, 251};
    CPLString hexColor = ngsRGBA2HEX(color);
    ngsRGBA newColor = ngsHEX2RGBA(hexColor);
    EXPECT_EQ(color.R, newColor.R);
    EXPECT_EQ(color.G, newColor.G);
    EXPECT_EQ(color.B, newColor.B);
    EXPECT_EQ(color.A, newColor.A);
}

TEST(CatalogTests, TestCatalogQuery) {
    initLib();

    CatalogObjectH catalog = ngsCatalogObjectGet("ngc://");
    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ASSERT_NE(pathInfo, nullptr);
    int count = 0;
    while(pathInfo[count].name) {
        count++;
    }
    ASSERT_GE(count, 1);
    std::string path2test = CPLSPrintf("ngc://%s", pathInfo[0].name); // Local connections
    ngsFree(pathInfo);

    CatalogObjectH path2testObject = ngsCatalogObjectGet(path2test.c_str());
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
    std::string catalogPath = ngsCatalogPathFromSystem(CPLGetCurrentDir());
    std::string zipPath = catalogPath + "/data/railway.zip";
    CatalogObjectH zipObject = ngsCatalogObjectGet(zipPath.c_str());

    pathInfo = ngsCatalogObjectQuery(zipObject, 0);
    ASSERT_NE(pathInfo, nullptr);
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
    initLib();

    std::string path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr, 0);
    std::string catalogPath = ngsCatalogPathFromSystem(path.c_str());
    ASSERT_STRNE(catalogPath.c_str(), "");

    char **options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_CONTAINER_DIR);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");

    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath.c_str());
    EXPECT_NE(ngsCatalogObjectCreate(catalog, "test_dir1", options),
              nullptr);
    EXPECT_NE(ngsCatalogObjectCreate(catalog, "test_dir1", options),
              nullptr);
    ngsListFree(options);
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

    options = ngsListAddNameIntValue(options, "TYPE", CAT_RASTER_TMS);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsListAddNameValue(options, "url", "http://tile.openstreetmap.org/{z}/{x}/{y}.png");
    options = ngsListAddNameValue(options, "epsg", "3857");
    options = ngsListAddNameValue(options, "z_min", "0");
    options = ngsListAddNameValue(options, "z_max", "19");

    EXPECT_NE(ngsCatalogObjectCreate(catalog, "osm.wconn", options),
              nullptr);

    pathInfo = ngsCatalogObjectQuery(catalog, 0);
    ASSERT_NE(pathInfo, nullptr);
    count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 3);
    ngsFree(pathInfo);

    // Test metadata
    std::string osmPath = ngsFormFileName(catalogPath.c_str(), "osm.wconn",
                                          nullptr, 1);
    CatalogObjectH osmRaster = ngsCatalogObjectGet(osmPath.c_str());
    EXPECT_EQ(ngsCatalogObjectSetProperty(osmRaster, "TMS_CACHE_EXPIRES", "555",
                                          ""), COD_SUCCESS);

    char **metadata = ngsCatalogObjectProperties(osmRaster, "");
    if(metadata != nullptr) {
        auto val = CSLFetchNameValue(metadata, "TMS_CACHE_EXPIRES");
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(EQUAL(val, "555"), 1);
    }

    ngsUnInit();
}

TEST(CatalogTests, TestAreaDownload) {
    initLib();
    auto path = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", nullptr, 0);
	auto catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");
    CatalogObjectH catalog = ngsCatalogObjectGet(catalogPath);

    char **options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_RASTER_TMS);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsListAddNameValue(options, "url", "http://bing.com/maps/default.aspx?cp={x}~{y}&lvl={z}&style=r");
    // options = ngsListAddNameValue(options, "url", "http://tile.openstreetmap.org/{z}/{x}/{y}.png");
    options = ngsListAddNameValue(options, "epsg", "3857");
    options = ngsListAddNameValue(options, "z_min", "0");
    options = ngsListAddNameValue(options, "z_max", "19");
    options = ngsListAddNameValue(options, "cache_expires", "300");

    EXPECT_NE(ngsCatalogObjectCreate(catalog, "cache_test.wconn", options),
              nullptr);

    ngsListFree(options);
    options = nullptr;

    // Test metadata
	auto osmPath = ngsFormFileName(catalogPath, "cache_test.wconn", nullptr, 1);
    CatalogObjectH osmRaster = ngsCatalogObjectGet(osmPath);
    EXPECT_EQ(ngsCatalogObjectOpen(osmRaster, nullptr), 1);
    char **metadata = ngsCatalogObjectProperties(osmRaster, "");
    if(metadata != nullptr) {
        auto val = CSLFetchNameValue(metadata, "TMS_CACHE_EXPIRES");
        ASSERT_NE(val, nullptr);
        EXPECT_EQ(EQUAL(val, "300"), 1);
    }

    // Download area
    options = ngsListAddNameValue(options, "MINX", "4183837.05");
    options = ngsListAddNameValue(options, "MINY", "7505200.05");
    options = ngsListAddNameValue(options, "MAXX", "4192825.05");
    options = ngsListAddNameValue(options, "MAXY", "7513067.05");
    options = ngsListAddNameValue(options, "ZOOM_LEVELS", "8,9");

    EXPECT_EQ(ngsRasterCacheArea(osmRaster, options, nullptr, nullptr),
              COD_SUCCESS);
    ngsListFree(options);
    options = nullptr;

    ngsUnInit();
}

TEST(MiscTests, TestURLRequest) {
	initLib();

    ngsURLRequestResult* result = ngsURLRequest(URT_GET,
                                               "https://ya.ru", nullptr,
                                                nullptr, nullptr);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->status, 0);
    ngsURLRequestResultFree(result);

    result = ngsURLRequest(URT_GET,
        "https://sandbox.nextgis.com/api/component/pyramid/pkg_version",
                           nullptr, nullptr, nullptr);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->status, 0);
    std::string data(reinterpret_cast<const char*>(result->data),
                   static_cast<size_t>(result->dataLen));
    std::cout << data << std::endl;

    ngsURLRequestResultFree(result);

    result = ngsURLRequest(URT_GET, NEXTGIS_URL, nullptr, nullptr, nullptr);

    ASSERT_NE(result, nullptr);
    EXPECT_GE(result->status, 0);

    ngsURLRequestResultFree(result);

    ngsUnInit();
}

TEST(MiscTests, TestJSONURLLoad) {
	initLib();

    JsonDocumentH doc = ngsJsonDocumentCreate();
    ASSERT_NE(doc, nullptr);
    resetCounter();
    EXPECT_EQ(ngsJsonDocumentLoadUrl(doc,
            "https://sandbox.nextgis.com/api/component/pyramid/pkg_version",
                                     nullptr, ngsTestProgressFunc, nullptr),
              COD_SUCCESS);
    EXPECT_GE(getCounter(), 1);

    JSONObjectH root = ngsJsonDocumentRoot(doc);
    ASSERT_NE(root, nullptr);

    JSONObjectH ngwVersion = ngsJsonObjectGetObject(root, "nextgisweb");
    ASSERT_NE(ngwVersion, nullptr);

    EXPECT_STRNE("0", ngsJsonObjectGetString(ngwVersion, "0"));

    ngsJsonObjectFree(ngwVersion);

    ngsJsonObjectFree(root);
    ngsJsonDocumentFree(doc);

    ngsUnInit();
}

TEST(MiscTests, TestBasicAuth) {
	initLib();

    JsonDocumentH doc = ngsJsonDocumentCreate();
    ASSERT_NE(doc, nullptr);
    resetCounter();
    EXPECT_EQ(ngsJsonDocumentLoadUrl(doc,
            "https://sandbox.nextgis.com/api/component/auth/current_user",
                                     nullptr, ngsTestProgressFunc, nullptr),
              COD_SUCCESS);

    JSONObjectH root = ngsJsonDocumentRoot(doc);
    ASSERT_NE(root, nullptr);

    EXPECT_STREQ(ngsJsonObjectGetStringForKey(root, "keyname", ""), "guest");

    ngsJsonObjectFree(root);
    ngsJsonDocumentFree(doc);

    char **authOptions = nullptr;
    authOptions = ngsListAddNameValue(authOptions, "type", "basic");
    authOptions = ngsListAddNameValue(authOptions, "login", "administrator");
    authOptions = ngsListAddNameValue(authOptions, "password", "demodemo");

    ngsURLAuthAdd(SANDBOX_URL, authOptions);
    ngsListFree(authOptions);

    doc = ngsJsonDocumentCreate();

    EXPECT_EQ(ngsJsonDocumentLoadUrl(doc,
            "https://sandbox.nextgis.com/api/component/auth/current_user",
                                     nullptr, ngsTestProgressFunc, nullptr),
              COD_SUCCESS);

    root = ngsJsonDocumentRoot(doc);
    ASSERT_NE(root, nullptr);

    EXPECT_STREQ(ngsJsonObjectGetStringForKey(root, "keyname", ""), "administrator");

    ngsJsonObjectFree(root);
    ngsJsonDocumentFree(doc);

    ngsUnInit();
}

TEST(MiscTests, TestCrypt) {
    const char *key = ngsGeneratePrivateKey();
    char **options = nullptr;
    options = ngsListAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsListAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr, 0));
    options = ngsListAddNameValue(options, "CRYPT_KEY", key);
    EXPECT_EQ(ngsInit(options), COD_SUCCESS);
    ngsListFree(options);

    const char *ptext = "Create your GIS in a couple of minutes using a web browser. Upload your geodata. Make an unlimited number of web maps. Share your geodata with friends and colleagues from any part of the world.";

    const char *ctext = ngsEncryptString(ptext);
    const char *rtext = ngsDecryptString(ctext);

    EXPECT_STREQ(ptext, rtext);

    const char *deviceId = ngsGetDeviceId(false);
    std::cout << "Device ID: " << deviceId << "\n";

    EXPECT_STRNE(deviceId, "");

    ngsUnInit();
}
