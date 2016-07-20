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
#include "rasterdataset.h"

static int counter = 0;

void ngsTestNotifyFunc(enum ngsSourceCodes /*src*/, const char* /*table*/, long /*row*/,
                       enum ngsChangeCodes /*operation*/) {
    counter++;
}

TEST(StoreTests, TestCreate) {
    EXPECT_EQ(ngsInit(nullptr, nullptr), ngsErrorCodes::SUCCESS);
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

TEST(StoreTests, TestDelete) {
    ngs::DataStorePtr storage = ngs::DataStore::open("./tmp/ngs.gpkg");
    EXPECT_NE(storage, nullptr);
    EXPECT_EQ(storage->destroy (), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(CSLCount(CPLReadDir("./tmp")), 2); // . and ..
    ngsUninit();
}


