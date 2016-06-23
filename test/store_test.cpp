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

void ngsTestNotifyFunc(char /*src*/, const char* /*table*/, long /*row*/,
                              enum ngsChangeCodes /*operation*/) {
    counter++;
}

TEST(StoreTests, TestCreate) {
    ngs::DataStore storage("./tmp", nullptr, nullptr);
    EXPECT_EQ(storage.create (), ngsErrorCodes::SUCCESS);
}

TEST(StoreTests, TestOpen) {
    ngs::DataStore storage("./tmp", nullptr, nullptr);
    EXPECT_EQ(storage.open (), ngsErrorCodes::SUCCESS);
}

TEST(StoreTests, TestCreateTMS){
    ngs::DataStore storage("./tmp", nullptr, nullptr);
    EXPECT_EQ(storage.open (), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(storage.datasetCount (), 0);
    counter = 0;
    storage.setNotifyFunc (ngsTestNotifyFunc);
    EXPECT_EQ(storage.createRemoteTMSRaster (TMS_URL, TMS_NAME, TMS_ALIAS,
                                             TMS_COPYING, TMS_EPSG, TMS_MIN_Z,
                                             TMS_MAX_Z, TMS_YORIG_TOP),
              ngsErrorCodes::SUCCESS);
    EXPECT_EQ(storage.datasetCount (), 1);
    EXPECT_GE(counter, 1);
}

TEST(StoreTests, TestDeleteTMS){
    ngs::DataStore storage("./tmp", nullptr, nullptr);
    EXPECT_EQ(storage.open (), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(storage.datasetCount (), 1);
    counter = 0;
    storage.setNotifyFunc (ngsTestNotifyFunc);
    ngs::DatasetPtr pDataset = storage.getDataset (TMS_NAME).lock ();
    ASSERT_NE(pDataset, nullptr);
    EXPECT_EQ(pDataset->destroy (), ngsErrorCodes::SUCCESS);
    EXPECT_EQ(storage.datasetCount (), 0);
    EXPECT_GE(counter, 1);
}

TEST(StoreTests, TestDelete) {
    ngs::DataStore storage("./tmp", nullptr, nullptr);
    EXPECT_EQ(storage.destroy (), ngsErrorCodes::SUCCESS);
}


