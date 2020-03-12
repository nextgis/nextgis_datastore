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
#ifndef NGSOBJECT_H
#define NGSOBJECT_H

#include <memory>
#include <string>
#include <vector>

#include "ngstore/codes.h"

#include "util/options.h"

namespace ngs {

class ObjectContainer;
class Object;
using ObjectPtr = std::shared_ptr<Object>;
using Properties = Options;

/**
 * @brief The base class for catalog items
 */
class Object
{
public:
    explicit Object(ObjectContainer * const parent = nullptr,
           const enum ngsCatalogObjectType type = CAT_UNKNOWN,
           const std::string &name = "",
           const std::string &path = "");
    virtual ~Object() = default;

    std::string name() const;
    std::string path() const;
    enum ngsCatalogObjectType type() const;
    ObjectContainer *parent() const;

    virtual std::string fullName() const;
    virtual bool destroy();
    virtual bool canDestroy() const;
    virtual bool rename(const std::string &newName);
    virtual bool canRename() const;
    virtual ObjectPtr pointer() const;
    virtual Properties properties(const std::string &domain) const;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const;
    virtual bool setProperty(const std::string &key,
                             const std::string &value,
                             const std::string &domain);
    virtual void deleteProperties(const std::string &domain);
    virtual bool sync();

protected:
    void setName(const std::string &value);
    void setPath(const std::string &value);

private:
    Object(Object const&) = delete;
    Object &operator= (Object const&) = delete;

protected:
    std::string m_name, m_path;
    ObjectContainer * const m_parent;
    enum ngsCatalogObjectType m_type;
};

}


#endif // NGSOBJECT_H
