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
#ifndef NGSOBJECTCONTAINER_H
#define NGSOBJECTCONTAINER_H

#include "object.h"

#include <vector>

#include "util/options.h"

namespace ngs {

class ObjectContainer : public Object
{
public:
    ObjectContainer(ObjectContainer * const parent = nullptr,
                    const ngsCatalogObjectType type = ngsCatalogObjectType::CAT_CONTAINER_ANY,
                    const CPLString & name = "",
                    const CPLString & path = "");
    virtual ~ObjectContainer() = default;
    virtual ObjectPtr getObject(const char* path);
    virtual void clear();
    virtual void refresh() {}
    virtual bool hasChildren() { return !m_children.empty(); }
    virtual bool canCreate(const ngsCatalogObjectType /*type*/) const {
        return false;
    }
    virtual bool create(const ngsCatalogObjectType /*type*/,
                        const CPLString & /*name*/,
                        const Options& /*options*/) {
        return false;
    }
    std::vector<ObjectPtr> getChildren() const;
    ObjectPtr getChild(const CPLString& name) const;
    virtual void addChild(ObjectPtr object) { m_children.push_back(object); }

// events
public:
    virtual void notifyChanges() { refresh(); }

protected:
    static void removeDuplicates(std::vector<const char*> &deleteNames,
                                 std::vector<const char*> &addNames);

protected:
    std::vector<ObjectPtr> m_children;
    bool m_childrenLoaded;
};

}

#endif // NGSOBJECTCONTAINER_H
