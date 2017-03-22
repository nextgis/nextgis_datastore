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

namespace catalog {


class Catalog : public ObjectContainer
{
public:
    static Catalog& instance()
    {
        static Catalog cat;
        return cat;
    }

protected:
    void init();

private:
    Catalog();
    virtual ~Catalog();

    Catalog(Catalog const&) = delete;
    Catalog& operator= (Catalog const&) = delete;
};

}

}

#endif // NGSCATALOG_H
