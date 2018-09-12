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
#ifndef NGSCATALOG_H
#define NGSCATALOG_H

#include "objectcontainer.h"
#include "factories/objectfactory.h"

#include <utility>

namespace ngs {

class Catalog;
using CatalogPtr = std::shared_ptr<Catalog>;

class Catalog : public ObjectContainer
{
public:
    Catalog();
    virtual ~Catalog() override = default;
    virtual std::string fullName() const override;
    virtual ObjectPtr getObject(const std::string &path) override;
    virtual void freeResources();
    virtual void createObjects(ObjectPtr object,
                               std::vector<std::string> &names);

    bool isFileHidden(const std::string &path, const std::string &name) const;
    void setShowHidden(bool value);
    virtual ObjectPtr getObjectBySystemPath(const std::string &path);
    virtual bool loadChildren() override;

    // Object interface
public:
    virtual ObjectPtr pointer() const override;

    // static
public:
    static void setInstance(Catalog *pointer);
    static CatalogPtr instance();
    static std::string separator();
    static unsigned short maxPathLength();
    static std::string toRelativePath(const Object *object,
                                      const ObjectContainer *objectContainer);
    static ObjectPtr fromRelativePath(const std::string &path,
                                      ObjectContainer *objectContainer);

private:
    Catalog(Catalog const&) = delete;
    Catalog &operator= (Catalog const&) = delete;

protected:
    bool m_showHidden;
    mutable std::vector<ObjectFactoryUPtr> m_factories;

};

}

#endif // NGSCATALOG_H
