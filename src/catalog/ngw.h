/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019 NextGIS, <info@nextgis.com>
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
#ifndef NGSNGWCONNECTION_H
#define NGSNGWCONNECTION_H

#include "objectcontainer.h"

namespace ngs {

class NGWResourceGroup : public ObjectContainer
{
public:
    explicit NGWResourceGroup(ObjectContainer * const parent,
                         const std::string &name,
                         const std::string &path);
};

class NGWConnection : public NGWResourceGroup
{
public:
    explicit NGWConnection(ObjectContainer * const parent,
                         const std::string &name,
                         const std::string &path);
    virtual bool loadChildren() override;
};

}

#endif // NGSNGWCONNECTION_H
