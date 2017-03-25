/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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

#include "ngstore/codes.h"
#include "catalog/folder.h"
#include "util/jsondocument.h"
#include "util/settings.h"

constexpr const char* SETTINGS_FILE = "./tmp/settings.json";

TEST(SettingsTests, WriteTest) {
    // just try to create directory
    ngs::Folder::mkDir("./tmp");

    ngs::JSONDocument doc;
    ngs::JSONObject root = doc.getRoot();
    root.add("one_level", true);
    root.add("two_level/second_level", false);
    root.add("three_level/second_level/third_level", true);
    root.add("four_level/second_level/third_level/forth_level", true);
    root.destroy("four_level/second_level/third_level/forth_level");
    root.add("three_level/second_level/third_level1", false);
    root.set("three_level/second_level/third_level1", true);
    EXPECT_EQ(doc.save(SETTINGS_FILE), ngsErrorCodes::EC_SUCCESS);
}

TEST(SettingsTests, ReadTest) {
    ngs::JSONDocument doc;
    ASSERT_EQ(doc.load(SETTINGS_FILE), ngsErrorCodes::EC_SUCCESS);
    ngs::JSONObject root = doc.getRoot();
    EXPECT_EQ(root.getBool("one_level", false), true);
    EXPECT_EQ(root.getBool("two_level/second_level", true), false);
    EXPECT_EQ(root.getBool("three_level/second_level/third_level", false), true);
    EXPECT_EQ(root.getBool("four_level/second_level/third_level/forth_level", false), false);
    EXPECT_EQ(root.getBool("three_level/second_level/third_level1", false), true);

    // delete settings file
    VSIUnlink(SETTINGS_FILE);
}

TEST(SettingsTests, Settings) {
    ngs::Settings &settings = ngs::Settings::instance();
    EXPECT_EQ(settings.getBool("catalog/show_hidden", false), false);
    settings.set("catalog/show_hidden", true);
    EXPECT_EQ(settings.getBool("catalog/show_hidden", false), true);
}
