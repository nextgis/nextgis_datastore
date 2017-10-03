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
#include "rasterfactory.h"

#include "ds/geometry.h"
#include "ds/raster.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

static const char* tifMainExts[] = {nullptr};
static const char* tifExtraExts[] = {"tfw", "tiffw", "wld", "tifw", "aux", "ovr",
                                     "tif.xml", "tiff.xml", "aux.xml",
                                     "ovr.aux.xml", "rrd", "xml", "lgo", "prj",
                                     "imd", "pvl", "att", "eph", "rpb", "rpc",
                                     nullptr};
constexpr FORMAT_EXT tifExt = {"tif", tifMainExts, tifExtraExts};
constexpr FORMAT_EXT tiffExt = {"tiff", tifMainExts, tifExtraExts};
static const char* tifAdds[] = {"_rpc.txt", "-browse.jpg", "_readme.txt", nullptr};

constexpr const char* KEY_TYPE = "type";
constexpr const char* KEY_X_MIN = "x_min";
constexpr const char* KEY_X_MAX = "x_max";
constexpr const char* KEY_Y_MIN = "y_min";
constexpr const char* KEY_Y_MAX = "y_max";
constexpr const char* KEY_LIMIT_X_MIN = "limit_x_min";
constexpr const char* KEY_LIMIT_X_MAX = "limit_x_max";
constexpr const char* KEY_LIMIT_Y_MIN = "limit_y_min";
constexpr const char* KEY_LIMIT_Y_MAX = "limit_y_max";

RasterFactory::RasterFactory()
{
    m_tiffSupported = Filter::getGDALDriver(CAT_RASTER_TIFF);
    m_wmstmsSupported =  Filter::getGDALDriver(CAT_RASTER_WMS) ||
            Filter::getGDALDriver(CAT_RASTER_TMS);

//    CAT_RASTER_BMP,
//    CAT_RASTER_TIL,
//    CAT_RASTER_IMG,
//    CAT_RASTER_JPEG,
//    CAT_RASTER_PNG,
//    CAT_RASTER_GIF,
//    CAT_RASTER_SAGA,
//    CAT_RASTER_VRT,
//    CAT_RASTER_POSTGIS,

}

const char* RasterFactory::getName() const
{
    return _("Rasters");
}

void RasterFactory::createObjects(ObjectContainer * const container,
                                  std::vector<const char *> * const names)
{
    nameExtMap nameExts;
    auto it = names->begin();
    while( it != names->end() ) {
        const char* ext = CPLGetExtension(*it);
        const char* baseName = CPLGetBasename(*it);
        nameExts[baseName].push_back(ext);
        ++it;
    }

    for(const auto& nameExtsItem : nameExts) {

        // Check if ESRI Shapefile
        if(m_tiffSupported) {
        FORMAT_RESULT result = isFormatSupported(
                    nameExtsItem.first, nameExtsItem.second, tifExt);
        if(result.isSupported) {
            const char* path = CPLFormFilename(container->path(), result.name,
                                               nullptr);
            checkAdditionalSiblings(container->path(), result.name, tifAdds,
                                    result.siblingFiles);
            addChild(container, result.name, path, CAT_RASTER_TIFF,
                     result.siblingFiles, names);
        }
        result = isFormatSupported(
                    nameExtsItem.first, nameExtsItem.second, tiffExt);
        if(result.isSupported) {
            const char* path = CPLFormFilename(container->path(), result.name,
                                               nullptr);
            checkAdditionalSiblings(container->path(), result.name, tifAdds,
                                    result.siblingFiles);
            addChild(container, result.name, path, CAT_RASTER_TIFF,
                     result.siblingFiles, names);
        }
        }

        if(m_wmstmsSupported && !nameExtsItem.second.empty()) {
            if(EQUAL(nameExtsItem.second[0], remoteConnectionExtension())) {
                CPLJSONDocument connectionFile;
                const char* path = CPLFormFilename(container->path(),
                                                   nameExtsItem.first,
                                                   remoteConnectionExtension());
                if(connectionFile.Load(path)) {
                    std::vector<CPLString> siblingFiles;
                    enum ngsCatalogObjectType type =
                            static_cast<enum ngsCatalogObjectType>(
                                connectionFile.GetRoot().GetInteger(
                                    KEY_TYPE, CAT_UNKNOWN));
                    addChild(container, nameExtsItem.first + "." +
                             remoteConnectionExtension(), path, type,
                             siblingFiles, names);
                }
            }
        }
    }
}

bool RasterFactory::createRemoteConnection(const enum ngsCatalogObjectType type,
                                           const char* path,
                                           const Options &options)
{
    switch(type) {
    case CAT_RASTER_TMS:
    {
        CPLString url = options.stringOption(KEY_URL);
        if(url.empty()) {
            return errorMessage(_("Missign required option 'url'"));
        }

        int epsg = options.intOption(KEY_EPSG, -1);
        if(epsg < 0) {
            return errorMessage(_("Missign required option 'epsg'"));
        }
        int z_min = options.intOption(KEY_Z_MIN, 0);
        int z_max = options.intOption(KEY_Z_MAX, 18);
        bool y_origin_top = options.boolOption(KEY_Y_ORIGIN_TOP, true);

        Envelope fullExtent;
        fullExtent.setMinX(options.doubleOption(KEY_X_MIN, DEFAULT_BOUNDS.minX()));
        fullExtent.setMaxX(options.doubleOption(KEY_X_MAX, DEFAULT_BOUNDS.maxX()));
        fullExtent.setMinY(options.doubleOption(KEY_Y_MIN, DEFAULT_BOUNDS.minY()));
        fullExtent.setMaxY(options.doubleOption(KEY_Y_MAX, DEFAULT_BOUNDS.maxY()));

        Envelope limitExtent;
        limitExtent.setMinX(options.doubleOption(KEY_LIMIT_X_MIN, DEFAULT_BOUNDS.minX()));
        limitExtent.setMaxX(options.doubleOption(KEY_LIMIT_X_MAX, DEFAULT_BOUNDS.maxX()));
        limitExtent.setMinY(options.doubleOption(KEY_LIMIT_Y_MIN, DEFAULT_BOUNDS.minY()));
        limitExtent.setMaxY(options.doubleOption(KEY_LIMIT_Y_MAX, DEFAULT_BOUNDS.maxY()));

        CPLJSONDocument connectionFile;
        CPLJSONObject root = connectionFile.GetRoot();
        root.Add(KEY_TYPE, type);
        root.Add(KEY_URL, url);
        root.Add(KEY_Z_MIN, z_min);
        root.Add(KEY_Z_MAX, z_max);
        root.Add(KEY_Y_ORIGIN_TOP, y_origin_top);
        root.Add(KEY_EXTENT, fullExtent.save());
        root.Add(KEY_LIMIT_EXTENT, limitExtent.save());
        root.Add(KEY_CACHE_EXPIRES, options.intOption(KEY_CACHE_EXPIRES,
                                                      defaultCacheExpires));
        root.Add(KEY_BAND_COUNT, options.intOption(KEY_BAND_COUNT, 4));
        CPLJSONObject user;
        for(auto it = options.begin(); it != options.end(); ++it) {
            if(EQUALN(it->first, KEY_USER_PREFIX, KEY_USER_PREFIX_LEN)) {
                user.Add(it->first.c_str() + KEY_USER_PREFIX_LEN, it->second);
            }
        }
        root.Add(KEY_USER, user);

        const char* newPath = CPLResetExtension(path,
                                    remoteConnectionExtension());
        return connectionFile.Save(newPath);
    }
    default:
        return errorMessage(_("Unsupported connection type %d"), type);
    }
}

void RasterFactory::addChild(ObjectContainer * const container,
                                    const CPLString &name,
                                    const CPLString &path,
                                    enum ngsCatalogObjectType subType,
                                    const std::vector<CPLString> &siblingFiles,
                                    std::vector<const char *> * const names)
{
    ObjectFactory::addChild(container,
                            ObjectPtr(new Raster(siblingFiles, container,
                                                 subType, name, path)));
    eraseNames(name, siblingFiles, names);
}

} // namespace ngs
