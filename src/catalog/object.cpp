/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2025 NextGIS, <info@nextgis.com>
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
#include "util/stringutil.h"

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
    if(m_parent) {
        m_parent->onChildDeleted(this);
    }
    return true;
}

bool Object::rename(const std::string &newName)
{
    ngsUnused(newName);
    return false;
}

ObjectContainer *Object::parent() const
{
    return m_parent;
}

Properties Object::properties(const std::string &domain) const
{
    if(domain.empty()) {
        Properties out;
        out.add("system_path", m_path);
        out.add("can_destroy", false);
        out.add("can_rename", false);
        out.add("can_sync", false);
        out.add("is_readonly", true);
        return out;
    }
    return Properties();
}

std::string Object::property(const std::string &key,
                             const std::string &defaultValue,
                             const std::string &domain) const
{
    if(domain.empty()) {
        if (compare(key, "system_path") ) {
            return m_path;
        }
        else if (compare(key, "can_destroy") ) {
            return "NO";
        }
        else if (compare(key, "can_rename") ) {
            return "NO";
        }
        else if (compare(key, "can_sync") ) {
            return "NO";
        }
        else if (compare(key, "is_readonly") ) {
            return "YES";
        }
    }
    return defaultValue;
}

bool Object::setProperty(const std::string &key, const std::string &value,
                         const std::string &domain)
{
    ngsUnused(key);
    ngsUnused(value);
    ngsUnused(domain);
    return false;
}

void Object::deleteProperties(const std::string &domain)
{
    ngsUnused(domain);
}

/**
 * @brief Object::sync. Sync changes to disk or remote server.
 * @param [in] type Merging type
 * @param [out] conflicts Syncing conflicts array
 * @param [in] progress Progress of syncing operation
 * @return true on success.
 */
bool Object::sync(ngsSyncMergeType type, 
    std::vector<ngsFeatureChange> conflicts, const Progress& progress)
{
    return true;
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
