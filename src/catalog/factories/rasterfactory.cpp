/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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

#include "catalog/file.h"
#include "ds/geometry.h"
#include "ds/raster.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

static const std::vector<std::string> tifMainExts;
static const std::vector<std::string> tifExtraExts = {
    "tfw", "tiffw", "wld", "tifw", "aux", "ovr", "tif.xml", "tiff.xml", "aux.xml",
    "ovr.aux.xml", "rrd", "xml", "lgo", "prj", "imd", "pvl", "att", "eph", "rpb",
    "rpc"
};

static const FORMAT_EXT tifExt = {"tif", tifMainExts, tifExtraExts};
static const FORMAT_EXT tiffExt = {"tiff", tifMainExts, tifExtraExts};
static const std::vector<std::string> tifAdds = {
    "_rpc.txt", "-browse.jpg", "_readme.txt"
};

constexpr const char *KEY_X_MIN = "x_min";
constexpr const char *KEY_X_MAX = "x_max";
constexpr const char *KEY_Y_MIN = "y_min";
constexpr const char *KEY_Y_MAX = "y_max";
constexpr const char *KEY_LIMIT_X_MIN = "limit_x_min";
constexpr const char *KEY_LIMIT_X_MAX = "limit_x_max";
constexpr const char *KEY_LIMIT_Y_MIN = "limit_y_min";
constexpr const char *KEY_LIMIT_Y_MAX = "limit_y_max";

RasterFactory::RasterFactory()
{
    m_tiffSupported = static_cast<bool>(Filter::getGDALDriver(CAT_RASTER_TIFF));
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

std::string RasterFactory::name() const
{
    return _("Raster");
}

void RasterFactory::createObjects(ObjectContainer * const container,
                                  std::vector<std::string> &names)
{
    nameExtMap nameExts;
    auto it = names.begin();
    while( it != names.end() ) {
        auto ext = File::getExtension(*it);
        auto baseName = File::getBaseName(*it);
        nameExts[baseName].push_back(ext);
        ++it;
    }

    for(const auto &nameExtsItem : nameExts) {
        if(m_tiffSupported) {
            FORMAT_RESULT result = isFormatSupported(
                        nameExtsItem.first, nameExtsItem.second, tifExt);
            if(result.isSupported) {
                std::string path = File::formFileName(container->path(),
                                                      result.name);
                checkAdditionalSiblings(container->path(), result.name, tifAdds,
                                        result.siblingFiles);
                addChildInternal(container, result.name, path, CAT_RASTER_TIFF,
                         result.siblingFiles, names);
            }
            result = isFormatSupported(
                        nameExtsItem.first, nameExtsItem.second, tiffExt);
            if(result.isSupported) {
                std::string path = File::formFileName(container->path(),
                                                      result.name);
                checkAdditionalSiblings(container->path(), result.name, tifAdds,
                                        result.siblingFiles);
                addChildInternal(container, result.name, path, CAT_RASTER_TIFF,
                         result.siblingFiles, names);
            }
        }

        if(m_wmstmsSupported && !nameExtsItem.second.empty()) {
            if(compare(nameExtsItem.second[0], Filter::extension(CAT_RASTER_TMS))) {
                std::string path = File::formFileName(
                            container->path(), nameExtsItem.first,
                            Filter::extension(CAT_RASTER_TMS));
                enum ngsCatalogObjectType type = typeFromConnectionFile(path);
                if(Filter::isRaster(type)) {
                    std::vector<std::string> siblingFiles;
                    addChildInternal(container, nameExtsItem.first + "." +
                         Filter::extension(type), path, type,
                         siblingFiles, names);
                }
            }
        }
    }
}

bool RasterFactory::createRemoteConnection(const enum ngsCatalogObjectType type,
                                           const std::string &path,
                                           const Options &options)
{
    switch(type) {
    case CAT_RASTER_TMS:
    {
        std::string url = options.asString(KEY_URL);
        if(url.empty()) {
            return errorMessage(_("Missing required option 'url'"));
        }

        int epsg = options.asInt(KEY_EPSG, NOT_FOUND);
        if(epsg < 0) {
            return errorMessage(_("Missing required option 'epsg'"));
        }
        int z_min = options.asInt(KEY_Z_MIN, 0);
        int z_max = options.asInt(KEY_Z_MAX, 18);
        bool y_origin_top = options.asBool(KEY_Y_ORIGIN_TOP, true);

        Envelope fullExtent;
        fullExtent.setMinX(options.asDouble(KEY_X_MIN, DEFAULT_BOUNDS.minX()));
        fullExtent.setMaxX(options.asDouble(KEY_X_MAX, DEFAULT_BOUNDS.maxX()));
        fullExtent.setMinY(options.asDouble(KEY_Y_MIN, DEFAULT_BOUNDS.minY()));
        fullExtent.setMaxY(options.asDouble(KEY_Y_MAX, DEFAULT_BOUNDS.maxY()));
        if(!fullExtent.isInit()) {
            fullExtent = DEFAULT_BOUNDS;
        }

        Envelope limitExtent;
        limitExtent.setMinX(options.asDouble(KEY_LIMIT_X_MIN, DEFAULT_BOUNDS.minX()));
        limitExtent.setMaxX(options.asDouble(KEY_LIMIT_X_MAX, DEFAULT_BOUNDS.maxX()));
        limitExtent.setMinY(options.asDouble(KEY_LIMIT_Y_MIN, DEFAULT_BOUNDS.minY()));
        limitExtent.setMaxY(options.asDouble(KEY_LIMIT_Y_MAX, DEFAULT_BOUNDS.maxY()));
        if(!limitExtent.isInit()) {
            limitExtent = DEFAULT_BOUNDS;
        }

        CPLJSONDocument connectionFile;
        CPLJSONObject root = connectionFile.GetRoot();
        root.Add(KEY_TYPE, type);
        root.Add(KEY_URL, url);
        root.Add(KEY_Z_MIN, z_min);
        root.Add(KEY_Z_MAX, z_max);
        root.Add(KEY_Y_ORIGIN_TOP, y_origin_top);
        root.Add(KEY_EXTENT, fullExtent.save());
        root.Add(KEY_LIMIT_EXTENT, limitExtent.save());
        root.Add(KEY_CACHE_EXPIRES, options.asInt(KEY_CACHE_EXPIRES,
                                                      defaultCacheExpires));
        root.Add(KEY_CACHE_MAX_SIZE, options.asInt(KEY_CACHE_MAX_SIZE,
                                                       defaultCacheMaxSize));
        root.Add(KEY_BAND_COUNT, options.asInt(KEY_BAND_COUNT, 3));
        CPLJSONObject user;
        for(auto it = options.begin(); it != options.end(); ++it) {
            if(comparePart(it->first, USER_PREFIX_KEY, USER_PREFIX_KEY_LEN)) {
                user.Add(it->first.substr(USER_PREFIX_KEY_LEN), it->second);
            }
        }
        root.Add(USER_KEY, user);

        std::string newPath = File::resetExtension(path, Filter::extension(type));
        return connectionFile.Save(newPath);
    }
    default:
        return errorMessage(_("Unsupported connection type %d"), type);
    }
}

void RasterFactory::addChildInternal(ObjectContainer * const container,
                                    const std::string &name,
                                    const std::string &path,
                                    enum ngsCatalogObjectType subType,
                                    const std::vector<std::string> &siblingFiles,
                                    std::vector<std::string> &names)
{
    ObjectFactory::addChild(container,
                            ObjectPtr(new Raster(siblingFiles, container,
                                                 subType, name, path)));
    eraseNames(name, siblingFiles, names);
}

} // namespace ngs
