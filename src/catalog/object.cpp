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
#include "object.h"

#include "api_priv.h"
#include "catalog.h"

namespace ngs {

Object::Object(ObjectContainer * const parent,
               const enum ngsCatalogObjectType type,
               const std::string &name,
               const std::string &path) :
    m_name(name),
    m_path(path),
    m_parent(parent),
    m_type(type)
{

}

std::string Object::name() const
{
    return m_name;
}

std::string Object::path() const
{
    return m_path;
}

enum ngsCatalogObjectType Object::type() const
{
    return m_type;
}

std::string Object::fullName() const
{
    std::string out;
    if(nullptr != m_parent) {
        out = m_parent->fullName();
    }
    out += Catalog::separator() + m_name;

    return out;
}

ObjectPtr Object::pointer() const
{
    for(const auto& child : m_parent->getChildren()) {
        if(child.get() == this) {
            return child;
        }
    }
    return ObjectPtr();
}

bool Object::destroy()
{
    return false;
}

bool Object::canDestroy() const
{
    return false;
}

bool Object::rename(const std::string &newName)
{
    ngsUnused(newName)
    return false;
}

bool Object::canRename() const
{
    return false;
}

ObjectContainer *Object::parent() const
{
    return m_parent;
}

Properties Object::properties(const std::string &domain) const
{
    ngsUnused(domain);
    return Properties();
}

std::string Object::property(const std::string &key,
                             const std::string &defaultValue,
                             const std::string &domain) const
{
    ngsUnused(key);
    ngsUnused(defaultValue);
    ngsUnused(domain);
    return "";
}

bool Object::setProperty(const std::string &key, const std::string &value,
                         const std::string &domain)
{
    ngsUnused(key)
    ngsUnused(value)
    ngsUnused(domain)
            return false;
}

void Object::deleteProperties(const std::string &domain)
{
    ngsUnused(domain);
}

void Object::setName(const std::string &value)
{
    m_name = value;
}

void Object::setPath(const std::string &value)
{
    m_path = value;
}


} // namespace ngs
