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
#include "util/settings.h"

// gdal
#include "cpl_vsi.h"

TEST(SettingsTests, WriteTest) {
    auto tmpPath = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", "", 0);
    // just try to create directory
    VSIMkdir(tmpPath, 0755);

    auto settingsFile = ngsFormFileName(tmpPath, "settings.json", "", 0);

    CPLJSONDocument doc;
    CPLJSONObject root = doc.GetRoot();
    root.Add("one_level", true);
    root.Add("two_level/second_level", false);
    root.Add("three_level/second_level/third_level", true);
    root.Add("four_level/second_level/third_level/forth_level", true);
    root.Delete("four_level/second_level/third_level/forth_level");
    root.Add("three_level/second_level/third_level1", false);
    root.Set("three_level/second_level/third_level1", true);
    EXPECT_EQ(doc.Save(settingsFile), true);
}

TEST(SettingsTests, ReadTest) {
    auto tmpPath = ngsFormFileName(ngsGetCurrentDirectory(), "tmp", "", 0);
    auto settingsFile = ngsFormFileName(tmpPath, "settings.json", "", 0);

    CPLJSONDocument doc;
    ASSERT_EQ(doc.Load(settingsFile), true);
    CPLJSONObject root = doc.GetRoot();
    EXPECT_EQ(root.GetBool("one_level", false), true);
    EXPECT_EQ(root.GetBool("two_level/second_level", true), false);
    EXPECT_EQ(root.GetBool("three_level/second_level/third_level", false), true);
    EXPECT_EQ(root.GetBool("four_level/second_level/third_level/forth_level", false), false);
    EXPECT_EQ(root.GetBool("three_level/second_level/third_level1", false), true);

    // delete settings file
    VSIUnlink(settingsFile);
}


TEST(SettingsTests, Settings) {
    initLib();

    EXPECT_STREQ(ngsSettingsGetString("catalog/show_hidden", "OFF"), "OFF");
    ngsSettingsSetString("catalog/show_hidden", "ON");
    EXPECT_STREQ(ngsSettingsGetString("catalog/show_hidden", "OFF"), "ON");

    ngsUnInit();
}
