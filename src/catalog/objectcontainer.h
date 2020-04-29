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
#ifndef NGSOBJECTCONTAINER_H
#define NGSOBJECTCONTAINER_H

#include "object.h"

#include "util/progress.h"

namespace ngs {

constexpr const char *URL_KEY = "URL";
constexpr unsigned short MAX_EQUAL_NAMES = 100;

class ObjectContainer : public Object
{
    friend class ObjectFactory;
public:
    explicit ObjectContainer(ObjectContainer * const parent = nullptr,
                    const enum ngsCatalogObjectType type = CAT_CONTAINER_ANY,
                    const std::string &name = "",
                    const std::string &path = "");
    virtual ~ObjectContainer() = default;
    virtual ObjectPtr getObject(const std::string &path);
    virtual void clear();
    virtual void refresh();
    virtual bool hasChildren() const;
    virtual bool isReadOnly() const;
    virtual bool canCreate(const enum ngsCatalogObjectType type) const;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type,
                        const std::string &name,
                        const Options &options = Options());
    virtual bool canPaste(const enum ngsCatalogObjectType type) const;
    virtual int paste(ObjectPtr child, bool move = false,
                      const Options &options = Options(),
                      const Progress &progress = Progress());

    std::vector<ObjectPtr> getChildren() const;
    virtual ObjectPtr getChild(const std::string &name) const;
    virtual bool loadChildren();
    virtual std::string createUniqueName(const std::string &name,
                                         bool isContainer = true,
                                         const std::string &add = "",
                                         int counter = 0) const;
    virtual bool hasChild(const std::string &name) const;

    // events
public:
    virtual void onChildDeleted(Object *child);
    virtual ObjectPtr onChildCreated(Object *child);

protected:
    /**
     * @brief addChild Executes from catalog factories to add new child to
     * container.
     * @param object The child object to add.
     */
    virtual void addChild(ObjectPtr object);

protected:
    static void removeDuplicates(std::vector<std::string> &deleteNames,
                                 std::vector<std::string> &addNames);

protected:
    mutable std::vector<ObjectPtr> m_children;
    bool m_childrenLoaded;
};

}

#endif // NGSOBJECTCONTAINER_H
