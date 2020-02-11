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

#include "ngstore/api.h"

#include <ctime>

TEST(NGWTests, TestResourceGroup) {
    initLib();

    // Create connection
    auto connection = createConnection("sandbox.nextgis.com");
    ASSERT_NE(connection, nullptr);

    // Create resource group
    time_t rawTime = std::time(nullptr);
    auto groupName = "ngstest_group_" + std::to_string(rawTime);

    auto group = createGroup(connection, groupName);
    ASSERT_NE(group, nullptr);

    // Rename resource group
    auto newName = groupName + "_2";
    EXPECT_EQ(ngsCatalogObjectRename(group, newName.c_str()), COD_SUCCESS);

    // Delete resource group
    EXPECT_EQ(ngsCatalogObjectDelete(group), COD_SUCCESS);

    // Delete connection
    EXPECT_EQ(ngsCatalogObjectDelete(connection), COD_SUCCESS);

    ngsUnInit();
}

TEST(NGWTests, TestVectorLayer) {
    initLib();

    // Create connection
    auto connection = createConnection("sandbox.nextgis.com");
    ASSERT_NE(connection, nullptr);

    // Create resource group
    time_t rawTime = std::time(nullptr);
    auto groupName = "ngstest_group_" + std::to_string(rawTime);

    auto group = createGroup(connection, groupName);
    ASSERT_NE(group, nullptr);

    // Create empty vector layer
    char** options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_NGW_VECTOR_LAYER);
    options = ngsListAddNameValue(options, "DESCRIPTION", "некое описание");
    options = ngsListAddNameValue(options, "GEOMETRY_TYPE", "POINT");
    options = ngsListAddNameValue(options, "FIELD_COUNT", "5");
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
    options = ngsListAddNameValue(options, "FIELD_4_TYPE", "STRING");
    options = ngsListAddNameValue(options, "FIELD_4_NAME", "невалидное имя");

    auto vectorLayer = ngsCatalogObjectCreate(group, "новый точечный слой", options);
    ngsListFree(options);

    ASSERT_NE(vectorLayer, nullptr);

    // Rename vector layer
    EXPECT_EQ(ngsCatalogObjectRename(vectorLayer, "новый точечный слой 2"), COD_SUCCESS);

    // Delete vector layer
    EXPECT_EQ(ngsCatalogObjectDelete(vectorLayer), COD_SUCCESS);

    // Delete resource group
    EXPECT_EQ(ngsCatalogObjectDelete(group), COD_SUCCESS);

    // Delete connection
    EXPECT_EQ(ngsCatalogObjectDelete(connection), COD_SUCCESS);

    ngsUnInit();
}

TEST(NGWTests, TestPaste) {
    initLib();

    // Create connection
    auto connection = createConnection("sandbox.nextgis.com");
    ASSERT_NE(connection, nullptr);

    // Create resource group
    time_t rawTime = std::time(nullptr);
    auto groupName = "ngstest_group_" + std::to_string(rawTime);
    auto group = createGroup(connection, groupName);
    ASSERT_NE(group, nullptr);

    // Paste vector layer
    resetCounter();
    char** options = nullptr;
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

    CatalogObjectH shape = getLocalFile("/data/railway-mini.zip/railway-mini.shp");
    EXPECT_EQ(ngsCatalogObjectCopy(shape, group, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);

    EXPECT_GE(getCounter(), 470);

    // Find loaded layer by name
    auto vectorLayer = ngsCatalogObjectGetByName(group, layerName, 1);
    ASSERT_NE(vectorLayer, nullptr);

    EXPECT_GE(ngsFeatureClassCount(vectorLayer), 470);

    // Rename vector layer
    EXPECT_EQ(ngsCatalogObjectRename(vectorLayer, "новый слой 3"), COD_SUCCESS);

    // Delete vector layer
    EXPECT_EQ(ngsCatalogObjectDelete(vectorLayer), COD_SUCCESS);

    // Delete resource group
    EXPECT_EQ(ngsCatalogObjectDelete(group), COD_SUCCESS);

    // Delete connection
    EXPECT_EQ(ngsCatalogObjectDelete(connection), COD_SUCCESS);

    ngsUnInit();
}

TEST(NGWTests, TestPasteMI) {
    initLib();

    // Create connection
    auto connection = createConnection("sandbox.nextgis.com");
    ASSERT_NE(connection, nullptr);

    // Create resource group
    time_t rawTime = std::time(nullptr);
    auto groupName = "ngstest_group_" + std::to_string(rawTime);
    auto group = createGroup(connection, groupName);
    ASSERT_NE(group, nullptr);

    // Paste MI tab layer with ogr style
    resetCounter();
    char** options = nullptr;
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
    options = ngsListAddNameValue(options, "OGR_STYLE_STRING_TO_FIELD", "TRUE");
    // If OVERWRITE is ON, existing layer will delete and new created.
    // If CREATE_UNIQUE the NEW_NAME or name will append counter, if such layer
    // already present.
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");

    // TODO: Add MI thematic maps (theme Legends) from wor file
//    options = ngsListAddNameValue(options, "WOR_STYLE", "...");

    CatalogObjectH tab = getLocalFile("/data/bld.tab");
    EXPECT_EQ(ngsCatalogObjectCopy(tab, group, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);

    EXPECT_GE(getCounter(), 5);

    // Find loaded layer by name
    auto vectorLayer = ngsCatalogObjectGetByName(group, layerName, 1);
    ASSERT_NE(vectorLayer, nullptr);

    EXPECT_GE(ngsFeatureClassCount(vectorLayer), 5);

    // Rename vector layer
    EXPECT_EQ(ngsCatalogObjectRename(vectorLayer, "новый слой 3"), COD_SUCCESS);

    // TODO: Create mapserver style for org_style field
//    <map>
//      <layer>
//      <styleitem>OGR_STYLE</styleitem>
//        <class>
//          <name>default</name>
//        </class>
//      </layer>
//    </map>

    // Delete vector layer
    EXPECT_EQ(ngsCatalogObjectDelete(vectorLayer), COD_SUCCESS);

    // Delete resource group
    EXPECT_EQ(ngsCatalogObjectDelete(group), COD_SUCCESS);

    // Delete connection
    EXPECT_EQ(ngsCatalogObjectDelete(connection), COD_SUCCESS);

    ngsUnInit();
}

