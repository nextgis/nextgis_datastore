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
#include "object.h"

#include "catalog.h"

namespace ngs {

Object::Object(ObjectContainer *const parent,
               const enum ngsCatalogObjectType type,
               const CPLString & name,
               const CPLString & path) :
    m_name(name),
    m_path(path),
    m_parent(parent),
    m_type(type)
{

}

CPLString Object::fullName() const
{
    CPLString out;
    if(nullptr != m_parent)
        out = m_parent->fullName();
    out += Catalog::separator() + m_name;

    return out;
}

ObjectPtr Object::pointer() const
{
    for(const auto& child : m_parent->getChildren()) {
        if(child.get() == this)
            return child;
    }
    return ObjectPtr();
}

} // namespace ngs
