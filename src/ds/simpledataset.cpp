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

SimpleDataset::SimpleDataset(enum ngsCatalogObjectType subType,
                             std::vector<std::string> siblingFiles,
                             ObjectContainer * const parent,
                             const std::string &name,
                             const std::string &path) :
    Dataset(parent, CAT_CONTAINER_SIMPLE, name, path),
    m_subType(subType),
    m_siblingFiles(siblingFiles)
{

}

std::vector<std::string> SimpleDataset::siblingFiles() const
{
    return m_siblingFiles;
}

ObjectPtr SimpleDataset::internalObject() const
{
    return m_children.empty() ? ObjectPtr() : m_children[0];
}

bool SimpleDataset::hasChildren() const
{
    return false; // Don't show only one child
}

bool SimpleDataset::canCreate(const enum ngsCatalogObjectType) const
{
    return false;
}

bool SimpleDataset::canPaste(const enum ngsCatalogObjectType) const
{
    return false;
}

enum ngsCatalogObjectType SimpleDataset::subType() const
{
    return m_subType;
}

bool SimpleDataset::destroy()
{
    clear();
    close();

    if(!File::deleteFile(m_path)) {
        return false;
    }

    for(const auto &siblingFile : m_siblingFiles) {
        std::string path = File::formFileName(m_parent->path(), siblingFile);
        if(Folder::isDir(path)) {
            Folder::rmDir(path);
        }
        else {
            File::deleteFile(path);
        }
    }

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(fullName(), ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

void SimpleDataset::fillFeatureClasses() const
{
    for(int i = 0; i < m_DS->GetLayerCount(); ++i) {
        OGRLayer *layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            std::string layerName = layer->GetName();
            // layer->GetLayerDefn()->GetGeomFieldCount() == 0
            SimpleDataset *parent = const_cast<SimpleDataset*>(this);
            if(geometryType == wkbNone) {
                m_children.push_back(
                    ObjectPtr(new Table(layer, parent, m_subType, layerName)));
            }
            else {
                m_children.push_back(
                    ObjectPtr(new FeatureClass(layer, parent, m_subType,
                                               layerName)));
            }
            break;
        }
    }
}

GDALDataset *SimpleDataset::createAdditionsDataset()
{
    GDALDataset *out = Dataset::createAdditionsDataset();
    if(out) {
        m_siblingFiles.push_back(
             File::resetExtension(m_path, Dataset::additionsDatasetExtension()));
        m_siblingFiles.push_back(
             File::resetExtension(m_path, Dataset::attachmentsFolderExtension()));
    }
    return out;
}

} // namespace ngs
