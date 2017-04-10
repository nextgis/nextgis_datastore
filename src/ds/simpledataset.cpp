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

namespace ngs {

SimpleDataset::SimpleDataset(ngsCatalogObjectType subType,
                             std::vector<CPLString> siblingFiles,
                             ObjectContainer * const parent,
                             const CPLString &name,
                             const CPLString &path) :
    Dataset(parent, ngsCatalogObjectType::CAT_CONTAINER_SIMPLE, name, path),
    m_subType(subType), m_siblingFiles(siblingFiles)
{

}

ObjectPtr SimpleDataset::getInternalObject() const
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

}




