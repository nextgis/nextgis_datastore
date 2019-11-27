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
#ifndef NGSREMOTECONNECTIONS_H
#define NGSREMOTECONNECTIONS_H

#include "objectcontainer.h"

namespace ngs {

class Connections : public ObjectContainer
{
public:
    explicit Connections(ObjectContainer * const parent,
                         const enum ngsCatalogObjectType type,
                         const std::string &name,
                         const std::string &path);
    virtual bool loadChildren() override;
    virtual bool canDestroy() const override;
    virtual void refresh() override;
    virtual bool canPaste(const enum ngsCatalogObjectType type) const override;
    virtual int paste(ObjectPtr child, bool move, const Options &options,
                      const Progress &progress) override;
};

class GISServerConnections : public Connections
{
public:
    explicit GISServerConnections(ObjectContainer * const parent,
                                  const std::string &path = "");
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type, 
        const std::string &name, const Options &options) override;
};

class DatabaseConnections : public Connections
{
public:
    explicit DatabaseConnections(ObjectContainer * const parent,
                                 const std::string &path = "");
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type, 
        const std::string &name, const Options &options) override;
};

class ConnectionBase
{
public:
    ConnectionBase();
    virtual ~ConnectionBase() = default;
    virtual bool isOpened() const;
    virtual bool open() = 0;
    virtual void close() = 0;
protected:
    bool m_opened;
};

}

#endif // NGSREMOTECONNECTIONS_H
