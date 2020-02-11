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
#include "simpledatasetfactory.h"

#include <algorithm>
#include <array>

#include "catalog/file.h"
#include "ds/simpledataset.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"

namespace ngs {

static const std::vector<std::string> shpMainExts = {"shx", "dbf"};
static const std::vector<std::string> shpExtraExts = {
    "sbn", "sbx", "cpg", "prj", "qix", "osf",
    Dataset::additionsDatasetExtension(), Dataset::attachmentsFolderExtension() };
static const FORMAT_EXT shpExt = {"shp", shpMainExts, shpExtraExts};

static const std::vector<std::string> tabMainExts = {"dat", "map", "id" };
static const std::vector<std::string> tabExtraExts = {
    "cpg", "ind", "qix", "osf", Dataset::additionsDatasetExtension(),
    Dataset::attachmentsFolderExtension() };
static const FORMAT_EXT tabExt = {"tab", tabMainExts, tabExtraExts};

static const std::vector<std::string> mifMainExts = {"mid"};
static const std::vector<std::string> mifExtraExts = {
    "cpg", "qix", "osf", Dataset::additionsDatasetExtension(),
    Dataset::attachmentsFolderExtension() };
static const FORMAT_EXT mifExt = {"mif", mifMainExts, mifExtraExts};

static const std::vector<std::string> geojsonMainExts;
static const std::vector<std::string> geojsonExtraExts = {
    "qix", "osf", Dataset::additionsDatasetExtension(),
    Dataset::attachmentsFolderExtension() };
static const FORMAT_EXT geojsonExt = {"geojson", geojsonMainExts, geojsonExtraExts};

SimpleDatasetFactory::SimpleDatasetFactory() : ObjectFactory()
{
    m_shpSupported = Filter::getGDALDriver(CAT_FC_ESRI_SHAPEFILE);
    m_miSupported = Filter::getGDALDriver(CAT_FC_MAPINFO_TAB);
    m_geojsonSupported = Filter::getGDALDriver(CAT_FC_GEOJSON);
}

std::string SimpleDatasetFactory::name() const
{
    return _("Feature classes and tables");
}

void SimpleDatasetFactory::createObjects(ObjectContainer * const container,
                                         std::vector<std::string> &names)
{
    nameExtMap nameExts;
    auto it = names.begin();
    while( it != names.end() ) {
        std::string ext = File::getExtension(*it);
        std::string baseName = File::getBaseName(*it);
        nameExts[baseName].push_back(ext);
        ++it;
    }

    for(const auto& nameExtsItem : nameExts) {

        // Check if ESRI Shapefile
        if(m_shpSupported) {
            FORMAT_RESULT result = isFormatSupported(nameExtsItem.first,
                                                     nameExtsItem.second, shpExt);
            if(result.isSupported) {
                std::string path = File::formFileName(container->path(),
                                                      result.name, "");
                addChildInternal(container, result.name, path,
                                 CAT_FC_ESRI_SHAPEFILE, result.siblingFiles,
                                 names);
            }
        }

        // Check if MapInfo tab
        if(m_miSupported) {
            FORMAT_RESULT result = isFormatSupported(nameExtsItem.first,
                                                     nameExtsItem.second, tabExt);
            if(result.isSupported) {
                std::string path = File::formFileName(container->path(),
                                                      result.name, "");
                addChildInternal(container, result.name, path,
                                 CAT_FC_MAPINFO_TAB, result.siblingFiles, names);
            }

            // Check if MapInfo mif/mid
            result = isFormatSupported(nameExtsItem.first, nameExtsItem.second,
                                       mifExt);
            if(result.isSupported) {
                std::string path = File::formFileName(container->path(),
                                                      result.name, "");
                addChildInternal(container, result.name, path,
                                 CAT_FC_MAPINFO_MIF, result.siblingFiles, names);
            }
        }

        // Check if GeoJSON
        if(m_geojsonSupported) {
            FORMAT_RESULT result = isFormatSupported(nameExtsItem.first,
                                                     nameExtsItem.second,
                                                     geojsonExt);
            if(result.isSupported) {
                std::string path = File::formFileName(container->path(),
                                                      result.name, "");
                addChildInternal(container, result.name, path, CAT_FC_GEOJSON,
                                 result.siblingFiles, names);
            }
        }
    }
}

void SimpleDatasetFactory::addChildInternal(ObjectContainer * const container,
                                    const std::string &name,
                                    const std::string &path,
                                    enum ngsCatalogObjectType subType,
                                    const std::vector<std::string> &siblingFiles,
                                    std::vector<std::string> &names)
{
    ObjectFactory::addChild(container,
                            ObjectPtr(new SimpleDataset(subType, siblingFiles,
                                                        container, name, path)));
    eraseNames(name, siblingFiles, names);
}

} // namespace ngs
