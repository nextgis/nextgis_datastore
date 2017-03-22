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

#include <utility>

namespace ngs {

class Catalog : public ObjectContainer
{
public:
    Catalog();
    virtual ~Catalog() = default;
    virtual CPLString getFullName() const;
    virtual ObjectPtr getObject(const char* path) const;
    virtual void freeResources();

public:
    static void setInstance(Catalog* pointer);
    static Catalog* getInstance();
    static CPLString getSeparator();
    static unsigned short getMaxPathLength();

protected:
    void init();
};

}

#endif // NGSCATALOG_H
