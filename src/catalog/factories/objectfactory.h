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
#ifndef NGSOBJECTFACTORY_H
#define NGSOBJECTFACTORY_H

#include <vector>
#include <utility>

#include "catalog/objectcontainer.h"

namespace ngs {

class ObjectFactory
{
public:
    ObjectFactory();
    virtual ~ObjectFactory() = default;
    virtual const char* getName() const = 0;
    virtual void createObjects(ObjectContainer* const container,
                               std::vector<const char *>* const names) = 0;

    bool getEnabled() const;
    void setEnabled(bool enabled);
protected:
    virtual void addChild(ObjectContainer * const container, ObjectPtr object) {
        container->addChild(object);
    }

private:
    bool m_enabled;
};

typedef std::unique_ptr< ObjectFactory > ObjectFactoryUPtr;

}

#endif // NGSOBJECTFACTORY_H
