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
#include "objectcontainer.h"

#include "api_priv.h"
#include "catalog.h"

namespace ngs {

ObjectContainer::ObjectContainer(const Object *parent,
                                 const ngsCatalogObjectType type,
                                 const CPLString &name,
                                 const CPLString &path) :
    Object(parent, type, name, path), childrenLoaded(false)
{

}

ObjectPtr ObjectContainer::getObject(const char *path)
{
    hasChildren();

    CPLString searchName;
    size_t pathLen = CPLStrnlen(path, Catalog::getMaxPathLength());
    CPLString separator = Catalog::getSeparator();

    // Get first element
    for(size_t i = 0; i < pathLen; ++i) {
        searchName += path[0];
        path++;
        if(EQUALN(path, separator, separator.size())) {
            path += separator.size();
            break;
        }
    }

    // Search child with name searchName
    for(const ObjectPtr& child : children) {
#ifdef _WIN32 // No case sensitive compare on Windows
        if(EQUAL(child->getName(), searchName)) {
#else
        if(child->getName().compare(searchName) == 0) {
#endif
            if(EQUAL(path, "")) {
                // No more path elements
                return child;
            }
            else {
                ObjectContainer* const container =
                        ngsDynamicCast(ObjectContainer, child);
                if(nullptr != container)
                    return container->getObject(path);
            }
        }
    }
    return ObjectPtr();
}

void ObjectContainer::addObject(ObjectPtr object)
{
    children.push_back(object);
}

void ObjectContainer::clear()
{
    children.clear();
    childrenLoaded = false;
}

ObjectPtr ObjectContainer::getChild(const CPLString &name) const
{
    for(const ObjectPtr& child : children) {
        if(child->getName() == name)
            return child;
    }
    return ObjectPtr();
}

}
