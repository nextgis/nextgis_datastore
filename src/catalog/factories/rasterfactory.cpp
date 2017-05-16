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

#include "ds/raster.h"
#include "ngstore/catalog/filter.h"

namespace ngs {

static const char *tifMainExts[] = {nullptr};
static const char *tifExtraExts[] = {"tfw", "tiffw", "wld", "tifw", "aux", "ovr", "tif.xml", "tiff.xml", "aux.xml", "ovr.aux.xml", "rrd", "xml", "lgo", "prj", "imd", "pvl", "att", "eph", "rpb", "rpc", nullptr};
constexpr FORMAT_EXT tifExt = {"tif", tifMainExts, tifExtraExts};
constexpr FORMAT_EXT tiffExt = {"tiff", tifMainExts, tifExtraExts};

//CPLString szRpcName = CPLString(CPLGetBasename(m_sPath)) + CPLString("_rpc.txt");
//szRpcName = CPLString(CPLGetBasename(m_sPath)) + CPLString("-browse.jpg");
//szRpcName = CPLString(CPLGetBasename(m_sPath)) + CPLString("_readme.txt");


RasterFactory::RasterFactory()
{
    m_tiffSupported = Filter::getGDALDriver(
                ngsCatalogObjectType::CAT_RASTER_TIFF);
    m_wmstmsSupported =  Filter::getGDALDriver(
                ngsCatalogObjectType::CAT_RASTER_WMS) ||
            Filter::getGDALDriver(ngsCatalogObjectType::CAT_RASTER_TMS);

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



const char *RasterFactory::getName() const
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
            const char* path = CPLFormFilename(container->getPath(), result.name,
                                               nullptr);
            addChild(container, result.name, path,
                     ngsCatalogObjectType::CAT_RASTER_TIFF,
                     result.siblingFiles, names);
        }
        result = isFormatSupported(
                    nameExtsItem.first, nameExtsItem.second, tiffExt);
        if(result.isSupported) {
            const char* path = CPLFormFilename(container->getPath(), result.name,
                                               nullptr);
            addChild(container, result.name, path,
                     ngsCatalogObjectType::CAT_RASTER_TIFF,
                     result.siblingFiles, names);
        }
        }

        if(m_wmstmsSupported && !nameExtsItem.second.empty()) {
            if(EQUAL(nameExtsItem.second[0], "wconn")) {
                // TODO: Open json here and create path xml
//                const char* path = CPLFormFilename(container->getPath(), nameExtsItem.first,
//                                                   nullptr);
//                addChild(container, nameExtsItem.first, path,
//                         ngsCatalogObjectType::CAT_RASTER_TIFF,
//                         result.siblingFiles, names);
            }
        }
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
