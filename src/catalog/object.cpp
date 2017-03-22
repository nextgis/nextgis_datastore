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

namespace ngs {

Object::Object(const Object * parent, const int type, const CPLString & name,
               const CPLString & path) :
    name(name),
    path(path),
    parent(parent),
    type(type)
{

}

Object::~Object()
{

}

const Object *Object::getParent() const
{
    return parent;
}

CPLString Object::getName() const
{
    return name;
}

void Object::setName(const CPLString &value)
{
    name = value;
}

CPLString Object::getPath() const
{
    return path;
}

void Object::setPath(const CPLString &value)
{
    path = value;
}

ngsCatalogObjectType Object::getType() const
{
    return type;
}

} // namespace ngs
