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
#include "connectionfactory.h"
#include "ngw.h"
#include "catalog/file.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

constexpr const char *KEY_URL = "url";
constexpr const char *KEY_LOGIN = "login";
constexpr const char *KEY_PASSWORD = "password";

ConnectionFactory::ConnectionFactory() : ObjectFactory()
{
    m_wmsSupported = Filter::getGDALDriver(CAT_CONTAINER_WMS);
    m_wfsSupported = Filter::getGDALDriver(CAT_CONTAINER_WFS);
    m_ngwSupported = Filter::getGDALDriver(CAT_CONTAINER_NGW);
    m_pgSupported = Filter::getGDALDriver(CAT_CONTAINER_POSTGRES);
}


std::string ConnectionFactory::name() const
{
    return _("Remote connections (Dadatabases, GIS Servers)");
}

static enum ngsCatalogObjectType typeFromConnectionFile(const std::string &path)
{
    enum ngsCatalogObjectType out = CAT_UNKNOWN;
    CPLJSONDocument connectionFile;
    if(connectionFile.Load(path)) {
        out = static_cast<enum ngsCatalogObjectType>(
                    connectionFile.GetRoot().GetInteger(KEY_TYPE, CAT_UNKNOWN));
    }
    return out;
}

void ConnectionFactory::createObjects(ObjectContainer * const container,
                                     std::vector<std::string> &names)
{
    auto it = names.begin();
    while( it != names.end() ) {
        std::string ext = File::getExtension(*it);
        if((m_wmsSupported || m_wfsSupported || m_ngwSupported) &&
            compare(ext, Filter::extension(CAT_CONTAINER_NGW))) {
            std::string path = File::formFileName(container->path(), *it);
            enum ngsCatalogObjectType type = typeFromConnectionFile(path);
            if(Filter::isConnection(type)) {
                // Create object from connection
                 addChild(container,
                          ObjectPtr(new NGWConnection(container, *it, path)));

                it = names.erase(it);
            }
        }
        else if(m_pgSupported &&
                compare(ext, Filter::extension(CAT_CONTAINER_POSTGRES))) {
            std::string path = File::formFileName(container->path(), *it);
            enum ngsCatalogObjectType type = typeFromConnectionFile(path);
            if(Filter::isConnection(type)) {
                // TODO: Create object from connection
                // addChild(container, ObjectPtr(new DataStore(container, *it, path)));

                it = names.erase(it);
            }
        }
        else {
            ++it;
        }
    }
}

bool ConnectionFactory::createRemoteConnection(const enum ngsCatalogObjectType type,
                                               const std::string &path,
                                               const Options &options)
{
    switch(type) {
    case CAT_CONTAINER_NGW:
    {
        std::string url = options.asString(KEY_URL);
        if(url.empty()) {
            return errorMessage(_("Missing required option 'url'"));
        }

        std::string login = options.asString("login");
        if(login.empty()) {
            login = "guest";
        }
        else {
            login = CPLString(login).Trim();
        }
        std::string password = options.asString("password");

        CPLJSONDocument connectionFile;
        CPLJSONObject root = connectionFile.GetRoot();
        root.Add(KEY_TYPE, type);
        root.Add(KEY_URL, url);
        root.Add(KEY_LOGIN, login);
        if(!password.empty()) {
            root.Add(KEY_PASSWORD, encrypt(password));
        }

        std::string newPath = File::resetExtension(path, Filter::extension(type));
        return connectionFile.Save(newPath);
    }
    default:
        return errorMessage(_("Unsupported connection type %d"), type);
    }
}

}
