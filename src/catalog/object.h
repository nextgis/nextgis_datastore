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

#include "util/progress.h"

namespace ngs {

class ObjectContainer;

/**
 * @brief The base class for catalog items
 */
class Object
{
public:
    Object(ObjectContainer * const parent = nullptr,
           const ngsCatalogObjectType type = ngsCatalogObjectType::CAT_UNKNOWN,
           const CPLString & name = "",
           const CPLString & path = "");
    virtual ~Object() = default;
    const CPLString &getName() const;
    const CPLString &getPath() const;
    ngsCatalogObjectType getType() const;
    virtual CPLString getFullName() const;
    virtual bool destroy() {return false;}
    virtual bool canDestroy() const {return false;}
    virtual bool rename(const CPLString &/*newName*/)  {return false;}
    virtual bool canRename() const  {return false;}
    virtual bool copy(const CPLString &/*destPath*/, const Progress &/*progress*/)  {return false;}
    virtual bool canCopy(const CPLString &/*destPath*/) const  {return false;}
    virtual bool move(const CPLString &/*destPath*/, const Progress &/*progress*/) {return false;}
    const ObjectContainer *getParent() const;

protected:
    void setName(const CPLString &value);
    void setPath(const CPLString &value);

private:
    Object(Object const&) = delete;
    Object& operator= (Object const&) = delete;

protected:
    CPLString m_name, m_path;
    ObjectContainer * const m_parent;
    ngsCatalogObjectType m_type;
};

typedef std::shared_ptr< Object > ObjectPtr;

}


#endif // NGSOBJECT_H
