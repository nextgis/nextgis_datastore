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
#ifndef NGSOBJECT_H
#define NGSOBJECT_H

#include <memory>

#include "cpl_string.h"

#include "ngstore/codes.h"
#include "util/progress.h"

namespace ngs {

class ObjectContainer;
class Object;
typedef std::shared_ptr<Object> ObjectPtr;
/**
 * @brief The base class for catalog items
 */
class Object
{
public:
    explicit Object(ObjectContainer * const parent = nullptr,
           const enum ngsCatalogObjectType type = CAT_UNKNOWN,
           const CPLString & name = "",
           const CPLString & path = "");
    virtual ~Object() = default;
    const CPLString &name() const { return m_name; }
    const CPLString &path() const { return m_path; }
    enum ngsCatalogObjectType type() const { return m_type; }
    virtual CPLString fullName() const;
    virtual bool destroy() { return false; }
    virtual bool canDestroy() const { return false; }
    virtual bool rename(const CPLString &/*newName*/)  { return false; }
    virtual bool canRename() const  { return false; }
    ObjectContainer* parent() const { return m_parent; }
    virtual ObjectPtr pointer() const;
    virtual char** metadata(const char* /*domain*/) const { return nullptr; }
    virtual bool setMetadataItem(const char* /*name*/, const char* /*value*/,
                                 const char* /*domain*/) { return false; }

protected:
    void setName(const CPLString &value) { m_name = value; }
    void setPath(const CPLString &value) { m_path = value; }

private:
    Object(Object const&) = delete;
    Object& operator= (Object const&) = delete;

protected:
    CPLString m_name, m_path;
    ObjectContainer * const m_parent;
    enum ngsCatalogObjectType m_type;
};

}


#endif // NGSOBJECT_H
