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
#ifndef FOLDERFACTORY_H
#define FOLDERFACTORY_H

#include "objectfactory.h"

namespace ngs {

class FolderFactory : public ObjectFactory
{
public:
    FolderFactory();

    // ObjectFactory interface
public:
    virtual std::string name() const override;
    virtual void createObjects(ObjectContainer * const container,
                               std::vector<std::string> &names) override;
private:
    bool m_zipSupported;
};

}

#endif // FOLDERFACTORY_H
