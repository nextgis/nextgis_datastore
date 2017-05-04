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

// gdal
#include "cpl_conv.h"


#include "ds/datastore.h"

static int counter = 0;

void ngsTestNotifyFunc(const char* /*uri*/, enum ngsChangeCode /*operation*/) {
    counter++;
}

int ngsTestProgressFunc(enum ngsCode /*status*/, double /*complete*/,
                        const char* /*message*/, void* /*progressArguments*/) {
    counter++;
    return TRUE;
}

/*
TEST(StoreTests, TestCreate) {
    EXPECT_EQ(ngsInit(nullptr, nullptr), ngsErrorCodes::EC_SUCCESS);
    const char* epsgPath = CPLFindFile ("gdal", "gcs.csv");
    EXPECT_NE(epsgPath, nullptr);

    OGRSpatialReference wgs;
    wgs.importFromEPSG (4326);
    OGRSpatialReference wm;
    wm.importFromEPSG (3857);
    EXPECT_EQ (wgs.IsSame (&wm), 0);

    ngs::DataStorePtr storage = ngs::DataStore::create("./tmp/ngs.gpkg");
    EXPECT_NE(storage, nullptr);
}

TEST(StoreTests, TestOpen) {
    ngs::DataStorePtr storage = ngs::DataStore::open("./tmp/ngs.gpkg");
    EXPECT_NE(storage, nullptr);
}

/* FIXME: test create and delete TMS
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
    // TODO: rasterCount EXPECT_EQ(storage->datasetCount (), 1);
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
*/
/*
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
