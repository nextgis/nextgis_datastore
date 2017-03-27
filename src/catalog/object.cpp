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

Object::Object(ObjectContainer *const parent, const ngsCatalogObjectType type,
               const CPLString & name,
               const CPLString & path) :
    m_name(name),
    m_path(path),
    m_parent(parent),
    m_type(type)
{

}

const ObjectContainer *Object::getParent() const
{
    return m_parent;
}

const CPLString &Object::getName() const
{
    return m_name;
}

void Object::setName(const CPLString &value)
{
    m_name = value;
}

const CPLString &Object::getPath() const
{
    return m_path;
}

void Object::setPath(const CPLString &value)
{
    m_path = value;
}

ngsCatalogObjectType Object::getType() const
{
    return m_type;
}

CPLString Object::getFullName() const
{
    CPLString out;
    if(nullptr != m_parent)
        out = m_parent->getFullName();
    out += Catalog::getSeparator() + m_name;

    return out;
}

} // namespace ngs
