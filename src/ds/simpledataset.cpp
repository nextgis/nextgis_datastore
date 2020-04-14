/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#include "simpledataset.h"

#include "catalog/file.h"
#include "catalog/folder.h"
#include "util/notify.h"

namespace ngs {


//------------------------------------------------------------------------------
// SingleDataset
//------------------------------------------------------------------------------
SingleDataset::SingleDataset(enum ngsCatalogObjectType subType,
                             ObjectContainer * const parent,
                             const std::string &name,
                             const std::string &path) :
    Dataset(parent, CAT_CONTAINER_SIMPLE, name, path),
    m_subType(subType)
{
}

ObjectPtr SingleDataset::internalObject()
{
    loadChildren();
    return m_children.empty() ? ObjectPtr() : m_children[0];
}


enum ngsCatalogObjectType SingleDataset::subType() const
{
    return m_subType;
}


Properties SingleDataset::properties(const std::string &domain) const
{
    auto out = Dataset::properties(domain);
    if(domain.empty()) {
        out.add("sub_type", std::to_string(m_subType));
    }
    return out;
}

std::string SingleDataset::property(const std::string &key,
                                         const std::string &defaultValue,
                                         const std::string &domain) const
{
    if(domain.empty()) {
        if(key == "sub_type") {
            return std::to_string(m_subType);
        }
    }
    return Dataset::property(key, defaultValue, domain);
}


//------------------------------------------------------------------------------
// SingleLayerDataset
//------------------------------------------------------------------------------
SingleLayerDataset::SingleLayerDataset(enum ngsCatalogObjectType subType,
                                       ObjectContainer * const parent,
                                       const std::string &name,
                                       const std::string &path) :
    SingleDataset(subType, parent, name, path),
    m_geometryType(wkbUnknown)
{

}

Properties SingleLayerDataset::properties(const std::string &domain) const
{
    auto out = SingleDataset::properties(domain);
    if(domain.empty()) {
        out.add("geometry_type",
                FeatureClass::geometryTypeName(m_geometryType,
                                               FeatureClass::GeometryReportType::OGC));
    }
    return out;
}

std::string SingleLayerDataset::property(const std::string &key,
                                         const std::string &defaultValue,
                                         const std::string &domain) const
{
    if(domain.empty()) {
        if(key == "geometry_type") {
            return FeatureClass::geometryTypeName(m_geometryType,
                                                  FeatureClass::GeometryReportType::OGC);
        }
    }
    return SingleDataset::property(key, defaultValue, domain);
}

//------------------------------------------------------------------------------
// SimpleDataset
//------------------------------------------------------------------------------
FileSingleLayerDataset::FileSingleLayerDataset(enum ngsCatalogObjectType subType,
                             std::vector<std::string> siblingFiles,
                             ObjectContainer * const parent,
                             const std::string &name,
                             const std::string &path) :
    SingleLayerDataset(subType, parent, name, path),
    m_siblingFiles(siblingFiles)
{

}

std::vector<std::string> FileSingleLayerDataset::siblingFiles() const
{
    return m_siblingFiles;
}

bool FileSingleLayerDataset::hasChildren() const
{
    return false; // Don't show only one child
}

bool FileSingleLayerDataset::canCreate(const enum ngsCatalogObjectType) const
{
    return false;
}

bool FileSingleLayerDataset::canPaste(const enum ngsCatalogObjectType) const
{
    return false;
}


bool FileSingleLayerDataset::destroy()
{
    clear();
    close();

    if(!File::deleteFile(m_path)) {
        return false;
    }

    for(const auto &siblingFile : m_siblingFiles) {
        std::string path = File::formFileName(m_parent->path(), siblingFile);
        if(Folder::isDir(path)) {
            if(!Folder::rmDir(path)) {
                return false;
            }
        }
        else {
            if(!File::deleteFile(path)) {
                return false;
            }
        }
    }

    return ObjectContainer::destroy();
}

void FileSingleLayerDataset::fillFeatureClasses() const
{
    for(int i = 0; i < m_DS->GetLayerCount(); ++i) {
        OGRLayer *layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            m_geometryType = layer->GetGeomType();
            std::string layerName = layer->GetName();
            // layer->GetLayerDefn()->GetGeomFieldCount() == 0
            auto parent = const_cast<FileSingleLayerDataset*>(this);
            if(m_geometryType == wkbNone) {
                m_children.push_back(
                    ObjectPtr(new Table(layer, parent, subType(), layerName)));
            }
            else {
                m_children.push_back(
                    ObjectPtr(new FeatureClass(layer, parent, subType(), layerName)));
            }
            break;
        }
    }
}

GDALDatasetPtr FileSingleLayerDataset::createAdditionsDataset()
{
    GDALDatasetPtr out = Dataset::createAdditionsDataset();
    if(out) {
        m_siblingFiles.push_back(
             File::resetExtension(m_path, Dataset::additionsDatasetExtension()));
        m_siblingFiles.push_back(
             File::resetExtension(m_path, Dataset::attachmentsFolderExtension()));
    }
    return out;
}

} // namespace ngs
