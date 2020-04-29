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
#include "catalog/file.h"
#include "catalog/ngw.h"
#include "ngstore/catalog/filter.h"
#include "util/authstore.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

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
                continue;
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
                continue;
            }
        }
        ++it;
    }
}

bool ConnectionFactory::createRemoteConnection(const enum ngsCatalogObjectType type,
                                               const std::string &path,
                                               const Options &options)
{
    switch(type) {
    case CAT_CONTAINER_NGW:
    {
        resetError();
        std::string url = options.asString(URL_KEY);
        if(url.empty()) {
            return errorMessage(_("Missing required option 'url'"));
        }

        bool isGuest = options.asBool(KEY_IS_GUEST);
        std::string login = options.asString(KEY_LOGIN);
        if(login.empty()) {
            login = "guest";
            isGuest = true;
        }
        else {
            std::string oldLogin(login);
            login = CPLString(login).Trim();
            if(!compare(oldLogin, login, true)) {
                warningMessage("Login was trimmed!");
            }
        }
        std::string password = options.asString(KEY_PASSWORD);

        CPLJSONDocument connectionFile;
        CPLJSONObject root = connectionFile.GetRoot();
        root.Add(KEY_TYPE, type);
        root.Add(URL_KEY, url);
        root.Add(KEY_LOGIN, login);
        root.Add(KEY_IS_GUEST, isGuest);
        if(!password.empty()) {
            root.Add(KEY_PASSWORD, encrypt(password));
        }

        return connectionFile.Save(path);
    }
    default:
        return errorMessage(_("Unsupported connection type %d"), type);
    }
}

bool ConnectionFactory::checkRemoteConnection(const enum ngsCatalogObjectType type,
                                              const Options &options)
{
    resetError();
    switch(type) {
    case CAT_CONTAINER_NGW:
    {
        std::string url = options.asString(URL_KEY);
        if(url.empty()) {
            return errorMessage(_("Missing required option 'url'"));
        }

        std::string login = options.asString(KEY_LOGIN);
        std::string password = options.asString(KEY_PASSWORD);
        bool isGuest = options.asBool(KEY_IS_GUEST, false);
        if(login.empty() || isGuest) {
            login = "guest";
            password = "";
        }
        else {
            std::string oldLogin(login);
            login = CPLString(login).Trim();
            if(!compare(oldLogin, login, true)) {
                warningMessage("Login was trimmed!");
            }
        }

        CPLStringList requestOptions;
        std::string headers = "Accept: */*";
        if(!isGuest || password.empty()) {
            Options authOptions;
            authOptions.add(KEY_TYPE, "basic");
            authOptions.add(KEY_LOGIN, login);
            authOptions.add(KEY_PASSWORD, password);
            // Fake url just for auth headers
            std::string tmpUrl = "http://" + random(5) + ".info";
            AuthStore::authAdd(tmpUrl, authOptions);
            std::string auth = AuthStore::authHeader(tmpUrl);
            AuthStore::authRemove(tmpUrl);
            if(!auth.empty()) {
                headers += "\r\n";
                headers += auth;
            }
        }
        requestOptions.AddNameValue("HEADERS", headers.c_str());
        requestOptions.AddNameValue("CONNECTTIMEOUT", "30");
        requestOptions.AddNameValue("TIMEOUT", "65");
        requestOptions.AddNameValue("MAX_RETRY", "5");
        requestOptions.AddNameValue("RETRY_DELAY", "5");

        CPLJSONDocument checkReq;
        if(!checkReq.LoadUrl(ngw::getCurrentUserUrl(url), requestOptions)) {
            return errorMessage(CPLGetLastErrorMsg());
        }

        CPLJSONObject root = checkReq.GetRoot();
        if(!root.IsValid()) {
            return errorMessage(_("Response is invalid"));
        }

        auto currentLogin = root.GetString("keyname");
        if(currentLogin == login) {
            return true;
        }

        return errorMessage(_("User '%s' failed to connect to %s."),
                            login.c_str(), url.c_str());
    }
    default:
        return errorMessage(_("Unsupported connection type %d"), type);
    }
}

}
