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

// gdal
#include "cpl_multiproc.h"
#include "cpl_vsi.h"
#include "cpl_string.h"

#include "ngstore/api.h"
#include "ngstore/version.h"
#include "ngstore/util/constants.h"

#include "api_priv.h"

static int counter = 0;

void ngsTestNotifyFunc(enum ngsSourceCodes /*src*/,
                       const char* /*table*/,
                       long /*row*/,
                       enum ngsChangeCodes /*operation*/) {
    counter++;
}

int ngsTestProgressFunc(unsigned int /*taskId*/,
                        double /*complete*/,
                        const char* /*message*/,
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

    EXPECT_NE(nullptr, ngsGetVersionString("formats"));
}


TEST(BasicTests, TestInlines) {
    ngsRGBA color = {254, 253, 252, 251};
    int hexColor = ngsRGBA2HEX (color);
    ngsRGBA newColor = ngsHEX2RGBA (hexColor);
    EXPECT_EQ(color.R, newColor.R);
    EXPECT_EQ(color.G, newColor.G);
    EXPECT_EQ(color.B, newColor.B);
    EXPECT_EQ(color.A, newColor.A);
}

TEST(BasicTests, TestCatalogQuery) {
    char** options = nullptr;
    options = CSLAddNameValue(options, "DEBUG_MODE", "ON");
    options = CSLAddNameValue(options, "SETTINGS_DIR",
                              CPLFormFilename(CPLGetCurrentDir(), "tmp", nullptr));
    EXPECT_EQ(ngsInit(options), ngsErrorCodes::EC_SUCCESS);

    CSLDestroy(options);
    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery("ngc://");
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        count++;
    }
    ASSERT_GE(count, 1);
    const char* path2test = CPLSPrintf("ngc://%s", pathInfo[0].name);
    CPLFree(pathInfo);

    pathInfo = ngsCatalogObjectQuery(path2test);
    ASSERT_NE(pathInfo, nullptr);
    count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << path2test << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 1);
    path2test = CPLSPrintf("%s/%s", path2test, pathInfo[0].name);
    CPLFree(pathInfo);

    pathInfo = ngsCatalogObjectQuery(path2test);
    ASSERT_NE(pathInfo, nullptr);
    count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << path2test << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 1);
    CPLFree(pathInfo);

    ngsUnInit();
}


TEST(BasicTests, TestCreate) {
    char** options = nullptr;
    options = CSLAddNameValue(options, "DEBUG_MODE", "ON");
    options = CSLAddNameValue(options, "SETTINGS_DIR",
                              CPLFormFilename(CPLGetCurrentDir(), "tmp", nullptr));
    // TODO: CACHE_DIR, GDAL_DATA, LOCALE
    EXPECT_EQ(ngsInit(options), ngsErrorCodes::EC_SUCCESS);
    CSLDestroy(options);
    options = nullptr;

    const char* path = CPLFormFilename(CPLGetCurrentDir(), "tmp", nullptr);
    const char* catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");

    options = CSLAddNameValue(options, "TYPE",
                              std::to_string(CAT_CONTAINER_DIR).c_str());
    options = CSLAddNameValue(options, "CREATE_UNIQUE", "ON");

    EXPECT_EQ(ngsCatalogObjectCreate(catalogPath, "test_dir1", options),
              ngsErrorCodes::EC_SUCCESS);
    EXPECT_EQ(ngsCatalogObjectCreate(catalogPath, "test_dir1", options),
              ngsErrorCodes::EC_SUCCESS);

    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(catalogPath);
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 2);

    ngsUnInit();
}

TEST(BasicTests, TestDelete) {
    char** options = nullptr;
    options = CSLAddNameValue(options, "DEBUG_MODE", "ON");
    options = CSLAddNameValue(options, "SETTINGS_DIR",
                              CPLFormFilename(CPLGetCurrentDir(), "tmp", nullptr));
    EXPECT_EQ(ngsInit(options), ngsErrorCodes::EC_SUCCESS);
    CSLDestroy(options);

    const char* path = CPLFormFilename(CPLGetCurrentDir(), "tmp", nullptr);
    CPLString catalogPath = ngsCatalogPathFromSystem(path);
    ASSERT_STRNE(catalogPath, "");
    CPLString delPath = CPLFormFilename(catalogPath, "test_dir1", nullptr);
    EXPECT_EQ(ngsCatalogObjectDelete(delPath), ngsErrorCodes::EC_SUCCESS);
    ngsCatalogObjectInfo* pathInfo = ngsCatalogObjectQuery(catalogPath);
    ASSERT_NE(pathInfo, nullptr);
    size_t count = 0;
    while(pathInfo[count].name) {
        std::cout << count << ". " << catalogPath << "/" <<  pathInfo[count].name << '\n';
        count++;
    }
    EXPECT_GE(count, 2);
    ngsUnInit();
}

/*
 * TestCreateDs
 * TestOpenDs
 * TestLoadDs
 * TestDeleteDs
    // TODO: create datastore -- EXPECT_EQ(ngsDataStoreInit("./tmp/ngs.gpkg"), ngsErrorCodes::EC_SUCCESS);

TEST(BasicTests, TestOpen) {
    EXPECT_EQ(ngsInit(nullptr, nullptr), ngsErrorCodes::EC_SUCCESS);
    EXPECT_EQ(ngsDataStoreInit(""), ngsErrorCodes::EC_OPEN_FAILED);
    EXPECT_EQ(ngsDataStoreInit("./tmp/ngs.gpkg"), ngsErrorCodes::EC_SUCCESS);
    ngsSetNotifyFunction (ngsTestNotifyFunc);
}

TEST(BasicTests, TestCreateTMS) {
    counter = 0;
    EXPECT_EQ(ngsCreateRemoteTMSRaster(TMS_URL, TMS_NAME, TMS_ALIAS, TMS_COPYING,
                                       TMS_EPSG, TMS_MIN_Z, TMS_MAX_Z,
                                       TMS_YORIG_TOP), ngsErrorCodes::EC_SUCCESS);
    CPLSleep(0.1);
    EXPECT_GE(counter, 1);
}

TEST(BasicTests, TestInitMap) {
    unsigned char mapId = ngsMapCreate(DEFAULT_MAP_NAME, "unit test", DEFAULT_EPSG,
                             DEFAULT_MIN_X, DEFAULT_MIN_Y, DEFAULT_MAX_X,
                             DEFAULT_MAX_Y);
    EXPECT_GE(mapId, 1);
    EXPECT_NE(ngsMapInit (2), ngsErrorCodes::EC_SUCCESS);
    EXPECT_EQ(ngsMapInit (mapId), ngsErrorCodes::EC_SUCCESS);
    /*no layers so function not executed
    counter = 0;
    EXPECT_EQ(ngsMapDraw(1, DS_NORMAL, ngsTestProgressFunc, nullptr), ngsErrorCodes::EC_SUCCESS);
    CPLSleep(0.2);
    EXPECT_GE(counter, 1);*//*
}

TEST(BasicTests, TestLoad) {
    counter = 0;
    EXPECT_EQ(ngsDataStoreLoad("test" ,"./data/bld.shp", "", nullptr, ngsTestProgressFunc,
                      nullptr), 1);
    CPLSleep(0.6);
    EXPECT_GE(counter, 1);
}

TEST(BasicTests, TestDelete) {
    EXPECT_EQ(ngsDataStoreDestroy ("./tmp/ngs.gpkg", nullptr), ngsErrorCodes::EC_SUCCESS);
    ngsUninit();
}
*/
