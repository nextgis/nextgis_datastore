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
#ifndef NGSCATALOG_H
#define NGSCATALOG_H

#include "objectcontainer.h"
#include "factories/objectfactory.h"

#include <utility>

namespace ngs {

class Catalog;
typedef std::shared_ptr< Catalog > CatalogPtr;


class Catalog : public ObjectContainer
{
public:
    Catalog();
    virtual ~Catalog() = default;
    virtual CPLString getFullName() const override;
    virtual ObjectPtr getObject(const char* path) override;
    virtual ObjectPtr getObjectByLocalPath(const char* path);
    virtual void freeResources();
    virtual void createObjects(ObjectPtr object, std::vector< const char *> names);
    virtual bool hasChildren() override;

    bool isFileHidden(const CPLString& path, const char* name);
    void setShowHidden(bool value);

    // Object interface
public:
    virtual ObjectPtr getPointer() const override;

    // static
public:
    static void setInstance(Catalog* pointer);
    static CatalogPtr getInstance();
    static CPLString getSeparator();
    static unsigned short getMaxPathLength();
    static CPLString toRelativePath(const Object* object,
                                    const ObjectContainer *objectContainer);
    static ObjectPtr fromRelativePath(const char* path,
                                      ObjectContainer *objectContainer);

private:
    Catalog(Catalog const&) = delete;
    Catalog& operator= (Catalog const&) = delete;

protected:
    bool m_showHidden;
    std::vector<ObjectFactoryUPtr> m_factories;

};

}

#endif // NGSCATALOG_H
