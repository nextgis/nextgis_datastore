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
#include "objectcontainer.h"

#include "api_priv.h"
#include "catalog.h"

#include "util/global.h"
#include "util/stringutil.h"

namespace ngs {

ObjectContainer::ObjectContainer(ObjectContainer * const parent,
                                 const enum ngsCatalogObjectType type,
                                 const std::string &name,
                                 const std::string &path) :
    Object(parent, type, name, path),
    m_childrenLoaded(false)
{

}

ObjectPtr ObjectContainer::getObject(const std::string &path)
{
    std::string separator = Catalog::separator();

    // check relative paths

    if(comparePart("..", path, 2)) {
        return m_parent->getObject(path.substr(2 + separator.size()));
    }

    if(!loadChildren()) {
        return ObjectPtr();
    }

    std::string searchName;
    std::string pathRight;

    // Get first element
    size_t pos = path.find(separator);
    if(pos != std::string::npos) {
        searchName = path.substr(0, pos);
        pathRight = path.substr(pos + 1);
    }
    else {
        searchName = path;
    }

    // Search child with name searchName
    for(const ObjectPtr &child : m_children) {
       if(compare(child->name(), searchName)) {
            if(pathRight.empty()) {
                // No more path elements
                return child;
            }
            else {
                ObjectContainer * const container =
                        ngsDynamicCast(ObjectContainer, child);
                if(nullptr != container && container->loadChildren()) {
                    return container->getObject(pathRight);
                }
            }
        }
    }
    return ObjectPtr();
}

void ObjectContainer::clear()
{
    m_children.clear();
    m_childrenLoaded = false;
}


void ObjectContainer::refresh()
{

}


bool ObjectContainer::hasChildren() const
{
    return !m_children.empty();
}


bool ObjectContainer::isReadOnly() const
{
    return true;
}

bool ObjectContainer::canCreate(const enum ngsCatalogObjectType type) const
{
    ngsUnused(type)
    return false;
}

bool ObjectContainer::create(const enum ngsCatalogObjectType type,
                    const std::string &name, const Options &options)
{
    ngsUnused(type)
    ngsUnused(name)
    ngsUnused(options)
    return false;
}

bool ObjectContainer::canPaste(const enum ngsCatalogObjectType type) const
{
    ngsUnused(type)
    return false;
}

int ObjectContainer::paste(ObjectPtr child, bool move, const Options &options,
                           const Progress &progress)
{
    ngsUnused(child)
    ngsUnused(move)
    ngsUnused(options)
    ngsUnused(progress)
    return COD_UNSUPPORTED;
}

ObjectPtr ObjectContainer::getChild(const std::string &name) const
{
    for(const ObjectPtr& child : m_children) {
        if(child->name() == name) {
            return child;
        }
    }
    return ObjectPtr();
}

bool ObjectContainer::loadChildren()
{
    return true;
}

void ObjectContainer::removeDuplicates(std::vector<std::string> &deleteNames,
                                       std::vector<std::string> &addNames)
{
    if(addNames.empty()) {
        return;
    }

    auto it = deleteNames.begin();
    while(it != deleteNames.end()) {
        auto itan = addNames.begin();
        bool deleteName = false;
        while(itan != addNames.end()) {
            if(EQUAL((*it).c_str(), (*itan).c_str())) {
                it = deleteNames.erase(it);
                itan = addNames.erase(itan);
                deleteName = true;
                break;
            }
            else {
                ++itan;
            }
        }
        if(!deleteName) {
            ++it;
        }
    }
}

std::vector<ObjectPtr> ObjectContainer::getChildren() const
{
    return m_children;
}

void ObjectContainer::addChild(ObjectPtr object)
{
    m_children.push_back(object);
}

void ObjectContainer::notifyChanges()
{
    refresh();
}

}
