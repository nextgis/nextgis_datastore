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
#ifndef NGSCONNECTIONFACTORY_H
#define NGSCONNECTIONFACTORY_H

#include "objectfactory.h"

namespace ngs {


class ConnectionFactory : public ObjectFactory
{
public:
    ConnectionFactory();

    // ObjectFactory interface
public:
    virtual std::string name() const override;
    virtual void createObjects(ObjectContainer * const container,
                               std::vector<std::string> &names) override;
    // static
public:
    static bool createRemoteConnection(const enum ngsCatalogObjectType type,
                                       const std::string &path,
                                       const Options &options);
    static bool checkRemoteConnection(const enum ngsCatalogObjectType type,
                                      const Options &options);
protected:
    bool m_wmsSupported, m_wfsSupported, m_ngwSupported, m_pgSupported;
};

}

#endif // NGSCONNECTIONFACTORY_H
