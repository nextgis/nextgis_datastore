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

#include "cpl_conv.h"

static int counter = 0;

void ngsTestNotifyFunc(const char* /*uri*/, enum ngsChangeCode /*operation*/)
{
    counter++;
}

int ngsTestProgressFunc(enum ngsCode /*status*/, double /*complete*/,
                        const char* /*message*/, void* /*progressArguments*/)
{
    counter++;
    return TRUE;
}

int ngsGDALProgressFunc(double /*dfComplete*/, const char */*pszMessage*/,
                        void */*pProgressArg*/)
{
    counter++;
    return TRUE;
}

void resetCounter()
{
    counter = 0;
}

int getCounter()
{
    return counter;
}

void initLib()
{
    char **options = nullptr;
    options = ngsListAddNameValue(options, "DEBUG_MODE", "ON");
    options = ngsListAddNameValue(options, "SETTINGS_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    options = ngsListAddNameValue(options, "CACHE_DIR",
                              ngsFormFileName(ngsGetCurrentDirectory(), "tmp",
                                              nullptr));
    EXPECT_EQ(ngsInit(options), COD_SUCCESS);

    ngsListFree(options);
}


CatalogObjectH createConnection(const std::string &url, const std::string &protocol)
{
    auto conn = ngsCatalogObjectGet("ngc://GIS Server connections");
    if(conn == nullptr) {
        return nullptr;
    }

    char **options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_CONTAINER_NGW);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");
    options = ngsListAddNameValue(options, "login", "guest");
    options = ngsListAddNameValue(options, "url", (protocol + url).c_str());
    options = ngsListAddNameValue(options, "is_guest", "ON");

    if(ngsCatalogCheckConnection(CAT_CONTAINER_NGW, options) != 1) {
        ngsListFree(options);
        return nullptr;
    }

    auto result = ngsCatalogObjectCreate(conn, url.c_str(), options);
    ngsListFree(options);

    return result;
}

CatalogObjectH createGroup(CatalogObjectH parent, const std::string &name)
{
    if(parent == nullptr) {
        return nullptr;
    }

    char **options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_NGW_GROUP);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");

    auto result = ngsCatalogObjectCreate(parent, name.c_str(), options);
    ngsListFree(options);

    return result;
}

CatalogObjectH getLocalFile(const std::string &name)
{
    std::string testPath = ngsGetCurrentDirectory();
    std::string catalogPath = ngsCatalogPathFromSystem(testPath.c_str());
    std::string shapePath = catalogPath + name;
    return ngsCatalogObjectGet(shapePath.c_str());
}

CatalogObjectH createMIStore(const std::string &name)
{
    std::string testPath = ngsGetCurrentDirectory();
    std::string tmpPath = ngsFormFileName(testPath.c_str(), "tmp", nullptr);
    std::string tmpCatalogPath = ngsCatalogPathFromSystem(tmpPath.c_str());

    if(tmpCatalogPath.empty()) {
        return nullptr;
    }

    char **options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", CAT_CONTAINER_MAPINFO_STORE);
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");

    CatalogObjectH catalog = ngsCatalogObjectGet(tmpCatalogPath.c_str());
    CatalogObjectH mistore = ngsCatalogObjectCreate(catalog, name.c_str(),
                                                  options);
    ngsListFree(options);
    if(nullptr == mistore) {
        return nullptr;
    }

    ngsCatalogObjectSetProperty(mistore, "ENCODING", "CP1251", "nga");

    // Debug output
    ngsCatalogObjectInfo *pathInfo = ngsCatalogObjectQuery(catalog, 0);
    if(nullptr != pathInfo) {
        size_t count = 0;
        while(pathInfo[count].name) {
            std::cout << count << ". " << tmpCatalogPath << "/" <<  pathInfo[count].name << '\n';
            count++;
        }
    }
    ngsFree(pathInfo);
    // End debug output

    return mistore;
}

void uploadMIToNGW(const std::string &miPath, const std::string &layerName,
                   CatalogObjectH group)
{
    // Paste MI tab layer with ogr style
    resetCounter();
    char **options = nullptr;
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
    // Set new layer name. If not set, the source layer name will use.
    options = ngsListAddNameValue(options, "NEW_NAME", layerName.c_str());
    options = ngsListAddNameValue(options, "OGR_STYLE_STRING_TO_FIELD", "TRUE");
    // If OVERWRITE is ON, existing layer will delete and new created.
    // If CREATE_UNIQUE the NEW_NAME or name will append counter, if such layer
    // already present.
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");

    // TODO: Add MI thematic maps (theme Legends) from wor file
    //    options = ngsListAddNameValue(options, "WOR_STYLE", "...");

    CatalogObjectH tab = getLocalFile(miPath);
    EXPECT_EQ(ngsCatalogObjectCopy(tab, group, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);

    EXPECT_GE(getCounter(), 5);
}

CatalogObjectH createStyle(CatalogObjectH parent, const std::string &name,
                           const std::string &description,
                           enum ngsCatalogObjectType type,
                           const std::string &styleData, bool isPath)
{
    char **options = nullptr;
    options = ngsListAddNameIntValue(options, "TYPE", type);
    options = ngsListAddNameValue(options, "DESCRIPTION", description.c_str());
    if(!styleData.empty()) {
        if(isPath) {
            options = ngsListAddNameValue(options, "STYLE_PATH", styleData.c_str());
        }
        else {
            options = ngsListAddNameValue(options, "STYLE_STRING", styleData.c_str());
        }
    }
    auto style = ngsCatalogObjectCreate(parent, name.c_str(), options);
    ngsListFree(options);
    return style;
}

void uploadRasterToNGW(const std::string &rasterPath, const std::string &rasterName,
                   CatalogObjectH group)
{
    // Paste MI tab layer with ogr style
    resetCounter();
    char **options = nullptr;
    // Add descritpion to NGW vector layer
    options = ngsListAddNameValue(options, "DESCRIPTION", "описание тест1");
    // Set new layer name. If not set, the source layer name will use.
    options = ngsListAddNameValue(options, "NEW_NAME", rasterName.c_str());
    options = ngsListAddNameValue(options, "CREATE_UNIQUE", "ON");

    CatalogObjectH raster = getLocalFile(rasterPath);
    EXPECT_EQ(ngsCatalogObjectCopy(raster, group, options,
                                   ngsTestProgressFunc, nullptr), COD_SUCCESS);

    EXPECT_GE(getCounter(), 1);
}
