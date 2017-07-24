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
#include "simpledataset.h"

#include "catalog/file.h"
#include "util/notify.h"

namespace ngs {

SimpleDataset::SimpleDataset(enum ngsCatalogObjectType subType,
                             std::vector<CPLString> siblingFiles,
                             ObjectContainer * const parent,
                             const CPLString &name,
                             const CPLString &path) :
    Dataset(parent, ngsCatalogObjectType::CAT_CONTAINER_SIMPLE, name, path),
    m_subType(subType), m_siblingFiles(siblingFiles)
{

}

ObjectPtr SimpleDataset::internalObject() const
{
    if(m_children.empty())
        return ObjectPtr();
    return m_children[0];
}

bool SimpleDataset::hasChildren()
{
    if(m_childrenLoaded)
        return false;

    Dataset::hasChildren();

    return false;
}

bool SimpleDataset::destroy()
{
    clear();
    GDALClose(m_DS);
    m_DS = nullptr;
    if(!File::deleteFile(m_path)) {
        return false;
    }

    for(const auto &siblingFile : m_siblingFiles) {
        const char* path = CPLFormFilename(m_parent->path(), siblingFile, nullptr);
        File::deleteFile(path);
    }

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(fullName(), ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

void SimpleDataset::fillFeatureClasses()
{
    for(int i = 0; i < m_DS->GetLayerCount(); ++i) {
        OGRLayer* layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            const char* layerName = layer->GetName();
            // layer->GetLayerDefn()->GetGeomFieldCount() == 0
            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new Table(layer, this,m_subType,
                                                         layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new FeatureClass(layer, this,
                                                                m_subType,
                                                                layerName)));
            }
            break;
        }
    }
}

} // namespace ngs






