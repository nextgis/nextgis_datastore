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
#include "ngw.h"

// GDAL
#include "cpl_http.h"

#include "api_priv.h"
#include "catalog.h"
#include "util/authstore.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/stringutil.h"
#include "file.h"

namespace ngs {


static CPLStringList getGDALHeaders(const std::string &url)
{
    CPLStringList out;
    std::string headers = "Accept: */*";
    std::string auth = AuthStore::authHeader(url);
    if(!auth.empty()) {
        headers += "\r\n";
        headers += auth;
    }
    out.AddNameValue("HEADERS", headers.c_str());
    return out;
}

//------------------------------------------------------------------------------
// NGWConnectionBase
//------------------------------------------------------------------------------

std::string NGWConnectionBase::connectionUrl() const
{
    return m_url;
}

bool NGWConnectionBase::isClsSupported(const std::string &cls) const
{
    return std::find(m_availableCls.begin(), m_availableCls.end(), cls) != m_availableCls.end();
}

//------------------------------------------------------------------------------
// NGWResourceBase
//------------------------------------------------------------------------------
NGWResourceBase::NGWResourceBase(NGWConnectionBase *connection,
                                 const std::string &resourceId) :
    m_connection(connection),
    m_resourceId(resourceId)
{

}

std::string NGWResourceBase::url() const
{
    if(m_connection) {
        return m_connection->connectionUrl();
    }
    return "";
}

bool NGWResourceBase::remove()
{
    return ngw::deleteResource(url(), m_resourceId, getGDALHeaders(url()).StealList());
}

//------------------------------------------------------------------------------
// NGWResource
//------------------------------------------------------------------------------
NGWResource::NGWResource(ObjectContainer * const parent,
                         const enum ngsCatalogObjectType type,
                         const std::string &name,
                         NGWConnectionBase *connection,
                         const std::string &resourceId) :
    Object(parent, type, name, ""),
    NGWResourceBase(connection, resourceId)
{
}

bool NGWResource::destroy()
{
    return NGWResourceBase::remove();
}

//------------------------------------------------------------------------------
// NGWResourceGroup
//------------------------------------------------------------------------------

NGWResourceGroup::NGWResourceGroup(ObjectContainer * const parent,
        const std::string &name,
        NGWConnectionBase *connection,
        const std::string &resourceId) :
    ObjectContainer(parent, CAT_CONTAINER_NGWGROUP, name, ""), // Don't need file system path
    NGWResourceBase(connection, resourceId)
{
    // Expected search API is available
    m_childrenLoaded = true;
}

ObjectPtr NGWResourceGroup::getResource(const std::string &resourceId) const
{
    if(m_resourceId == resourceId) {
        return pointer();
    }

    for(const ObjectPtr &child : m_children) {
        NGWResourceGroup *group = ngsDynamicCast(NGWResourceGroup, child);
        if(group != nullptr) {
            ObjectPtr resource = group->getResource(resourceId);
            if(resource) {
                return resource;
            }
        }
    }
    return ObjectPtr();
}

void NGWResourceGroup::addResource(const CPLJSONObject &resource)
{
    std::string cls = resource.GetString("resource/cls");
    std::string name = resource.GetString("resource/display_name");
    std::string id = resource.GetString("resource/id");

    if(cls == "resource_group") {
        addChild(ObjectPtr(new NGWResourceGroup(this, name, m_connection, id)));
    }
    else if(cls == "trackers_group") {
        addChild(ObjectPtr(new NGWTrackersGroup(this, name, m_connection, id)));
    }
}

bool NGWResourceGroup::canCreate(const enum ngsCatalogObjectType type) const
{
    if(m_connection == nullptr) {
        return false; // Not expected than connection is null
    }
    switch (type) {
    case CAT_CONTAINER_NGWGROUP:
        return m_connection->isClsSupported("resource_group");
    case CAT_CONTAINER_NGWTRACKERGROUP:
        return m_connection->isClsSupported("trackers_group");
    default:
        return false;
    }
}

bool NGWResourceGroup::destroy()
{
    return NGWResourceBase::remove();
}

ObjectPtr NGWResourceGroup::create(const enum ngsCatalogObjectType type,
                    const std::string &name, const Options &options)
{
    std::string newName = name;
    if(options.asBool("CREATE_UNIQUE")) {
        newName = createUniqueName(newName, false);
    }

    ObjectPtr child = getChild(newName);
    if(child) {
        if(options.asBool("OVERWRITE")) {
            if(!child->destroy()) {
                errorMessage(_("Failed to overwrite %s\nError: %s"),
                    newName.c_str(), getLastError());
                return ObjectPtr();
            }
        }
        else {
            errorMessage(_("Resource %s already exists. Add overwrite option or create_unique option to create resource here"),
                newName.c_str());
            return ObjectPtr();
        }
    }

    CPLJSONObject payload;
    CPLJSONObject resource("resource", payload);
    resource.Add("cls", ngw::objectTypeToNGWClsType(type));
    resource.Add("display_name", newName);
    std::string key = options.asString("KEY");
    if(!key.empty()) {
        resource.Add("keyname", key);
    }
    std::string desc = options.asString("DESCRIPTION");
    if(!desc.empty()) {
        resource.Add("description", desc);
    }
    CPLJSONObject parent("parent", resource);
    parent.Add("id", atoi(m_resourceId.c_str()));

    std::string resourceId = ngw::createResource(url(),
        payload.Format(CPLJSONObject::Plain), getGDALHeaders(url()).StealList());
    if(compare(resourceId, "-1", true)) {
        return ObjectPtr();
    }

    child = ObjectPtr();
    if(m_childrenLoaded) {
        switch(type) {
        case CAT_CONTAINER_NGWGROUP:
            child = ObjectPtr(new NGWResourceGroup(this, newName, m_connection, resourceId));
            m_children.push_back(child);
            break;
        case CAT_CONTAINER_NGWTRACKERGROUP:
            child = ObjectPtr(new NGWTrackersGroup(this, newName, m_connection, resourceId));
            m_children.push_back(child);
            break;
        default:
            return ObjectPtr();
        }
    }

    std::string newPath = fullName() + Catalog::separator() + newName;
    Notify::instance().onNotify(newPath, ngsChangeCode::CC_CREATE_OBJECT);

    return child;
}

//------------------------------------------------------------------------------
// NGWTrackersGroup
//------------------------------------------------------------------------------

NGWTrackersGroup::NGWTrackersGroup(ObjectContainer * const parent,
        const std::string &name,
        NGWConnectionBase *connection,
        const std::string &resourceId) :
    NGWResourceGroup(parent, name, connection, resourceId)
{
    m_type = CAT_CONTAINER_NGWTRACKERGROUP;
}

bool NGWTrackersGroup::canCreate(const enum ngsCatalogObjectType type) const
{
    if(type == CAT_NGW_TRACKER && m_connection) {
        return m_connection->isClsSupported("tracker");
    }
    return false;
}

ObjectPtr NGWTrackersGroup::create(const enum ngsCatalogObjectType type,
                    const std::string &name, const Options &options)
{
    std::string newName = name;
    if(options.asBool("CREATE_UNIQUE")) {
        newName = createUniqueName(newName, false);
    }

    ObjectPtr child = getChild(newName);
    if(child) {
        if(options.asBool("OVERWRITE")) {
            if(!child->destroy()) {
                errorMessage(_("Failed to overwrite %s\nError: %s"),
                    newName.c_str(), getLastError());
                return ObjectPtr();
            }
        }
        else {
            errorMessage(_("Resource %s already exists. Add overwrite option or create_unique option to create resource here"),
                newName.c_str());
            return ObjectPtr();
        }
    }

    CPLJSONObject payload;
    CPLJSONObject resource("resource", payload);
    resource.Add("cls", ngw::objectTypeToNGWClsType(type));
    resource.Add("display_name", newName);
    std::string key = options.asString("KEY");
    if(!key.empty()) {
        resource.Add("keyname", key);
    }
    std::string desc = options.asString("DESCRIPTION");
    if(!desc.empty()) {
        resource.Add("description", desc);
    }

    CPLJSONObject parent("parent", resource);
    parent.Add("id", atoi(m_resourceId.c_str()));

    CPLJSONObject tracker("tracker", payload);
    std::string tracker_id = options.asString("TRACKER_ID");
    tracker.Add("unique_id", tracker_id);
    std::string tracker_desc = options.asString("TRACKER_DESCRIPTION");
    if(!tracker_desc.empty()) {
        tracker.Add("description", tracker_desc);
    }
    else {
        tracker.Add("description", "");
    }
    std::string tracker_type = options.asString("TRACKER_TYPE", "ng_mobile");
    tracker.Add("device_type", tracker_type);

    std::string tracker_fuel = options.asString("TRACKER_FUEL");
    if(!tracker_fuel.empty()) {
        tracker.Add("consumption_lpkm", atof(tracker_fuel.c_str()));
    }
    else {
        tracker.SetNull("consumption_lpkm");
    }

    tracker.Add("is_registered", "");

    std::string resourceId = ngw::createResource(url(),
        payload.Format(CPLJSONObject::Plain), getGDALHeaders(url()).StealList());
    if(compare(resourceId, "-1", true)) {
        return ObjectPtr();
    }

    if(m_childrenLoaded) {
        switch(type) {
        case CAT_NGW_TRACKER:
            child = ObjectPtr(new NGWResource(this, type, newName, m_connection, resourceId));
            m_children.push_back(child);
            break;
        default:
            return ObjectPtr();
        }
    }

    std::string newPath = fullName() + Catalog::separator() + newName;
    Notify::instance().onNotify(newPath, ngsChangeCode::CC_CREATE_OBJECT);

    return child;
}

//------------------------------------------------------------------------------
// NGWConnection
//------------------------------------------------------------------------------

NGWConnection::NGWConnection(ObjectContainer * const parent,
                         const std::string &name,
                         const std::string &path) :
    NGWResourceGroup(parent, name)
{
    m_type = CAT_CONTAINER_NGW;
    m_path = path;
    m_connection = this;
    m_childrenLoaded =false;
}

NGWConnection::~NGWConnection()
{
    AuthStore::authRemove(m_url);
}

bool NGWConnection::loadChildren()
{
    if(m_childrenLoaded) {
        return true;
    }

    fillProperties();

    if(!m_url.empty()) {
        fillCapabilities();
        if(!m_searchApiUrl.empty()) {
            CPLJSONDocument searchReq;
            if(searchReq.LoadUrl(m_searchApiUrl + "?serialization=full", getGDALHeaders(m_url))) {
                CPLJSONArray root(searchReq.GetRoot());
                if(root.IsValid()) {
                    m_childrenLoaded = true;
                    std::map<std::string, CPLJSONObject> noParentResources;
                    for(int i = 0; i < root.Size(); ++i) {
                        CPLJSONObject resource = root[i];
                        std::string resourceId =
                                resource.GetString("resource/parent/id", "-1");
                        if(resourceId != "-1") {
                            ObjectPtr parent = getResource(resourceId);
                            if(parent) {
                                NGWResourceGroup *group = ngsDynamicCast(NGWResourceGroup, parent);
                                if(group) {
                                    group->addResource(resource);
                                }
                            }
                            else {
                                noParentResources[resourceId] = resource;
                            }
                        }
                    }

                    int counter = static_cast<int>(noParentResources.size() * 1.3);
                    while(!noParentResources.empty()) {
                        auto it = noParentResources.begin();
                        ObjectPtr parent;
                        while(it != noParentResources.end()) {
                            if((parent = getResource(it->first))) {
                                break;
                            }
                            ++it;
                        }

                        if(parent) {
                            NGWResourceGroup *group = ngsDynamicCast(NGWResourceGroup, parent);
                            if(group) {
                                group->addResource(it->second);
                            }
                            noParentResources.erase(it);
                        }

                        // Protect from infinity loop.
                        counter--;
                        if(counter < 0) {
                            break;
                        }
                    }
                }
            }
        }
    }
    return true;
}

void NGWConnection::fillCapabilities()
{
    // Check NGW version. Paging available from 3.1
    CPLJSONDocument routeReq;
    if(routeReq.LoadUrl( ngw::getRouteUrl(m_url), getGDALHeaders(m_url) )) {
        CPLJSONObject root = routeReq.GetRoot();
        if(root.IsValid()) {
            CPLJSONArray search = root.GetArray("resource.search");
            if(search.IsValid()) {
                m_searchApiUrl = m_url + search[0].ToString();
                CPLDebug("ngstore", "Search API URL: %s", m_searchApiUrl.c_str());
            }

            CPLJSONArray version = root.GetArray("pyramid.pkg_version");
            if(version.IsValid()) {
                m_versionApiUrl = m_url + version[0].ToString();
                CPLDebug("ngstore", "Version API URL: %s", m_versionApiUrl.c_str());
            }
        }
    }
    CPLJSONDocument schemaReq;
    if(schemaReq.LoadUrl( ngw::getSchemaUrl(m_url), getGDALHeaders(m_url) )) {
        CPLJSONObject root = schemaReq.GetRoot();
        if(root.IsValid()) {
            CPLJSONObject resources = root.GetObj("resources");
            for(auto &resource : resources.GetChildren()) {
                std::string type = resource.GetName();
                m_availableCls.push_back(type);
            }
        }
    }
}

bool NGWConnection::destroy()
{
    if(!File::deleteFile(m_path)) {
        return errorMessage(_("Failed to delete %s"), m_name.c_str());
    }

    std::string name = fullName();
    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(name, ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

void NGWConnection::fillProperties() const
{
    if(m_url.empty() || m_user.empty()) {
        CPLJSONDocument connectionFile;
        if(connectionFile.Load(m_path)) {
            CPLJSONObject root = connectionFile.GetRoot();
            m_url = root.GetString(KEY_URL);

            m_user = root.GetString(KEY_LOGIN);
            if(!m_user.empty() && !compare(m_user, "guest")) {
                std::string password = decrypt(root.GetString(KEY_PASSWORD));

                Options options;
                options.add("type", "basic");
                options.add("login", m_user);
                options.add("password", password);
                AuthStore::authAdd(m_url, options);
            }
        }
    }
}

Properties NGWConnection::properties(const std::string &domain) const
{
    fillProperties();
    if(domain.empty()) {
        Properties out;
        out.add("system_path", m_path);
        out.add("url", m_url);
        out.add("login", m_user);
        out.add("is_guest", compare(m_user, "guest") ? "yes" : "no");
        return out;
    }
    return Properties();
}

std::string NGWConnection::property(const std::string &key,
                             const std::string &defaultValue,
                             const std::string &domain) const
{
    fillProperties();
    if(domain.empty()) {
        if(key == "url") {
            return m_url;
        }

        if(key == "system_path") {
            return m_path;
        }

        if(key == "login") {
            return m_user;
        }

        if(key == "is_guest") {
            return compare(m_user, "guest") ? "yes" : "no";
        }
    }
    return defaultValue;
}

bool NGWConnection::setProperty(const std::string &key, const std::string &value,
        const std::string &domain)
{
    if(!domain.empty()) {
        return NGWResourceGroup::setProperty(key, value, domain);
    }

    if(key == "url") {
        AuthStore::authRemove(m_url);
        m_url = value;
    }

    if(key == "login") {
        AuthStore::authRemove(m_url);
        m_user = value;
    }

    if(key == "is_guest") {
        if(compare(value, "yes")) {
            if(!compare(m_user, "guest")) {
                AuthStore::authRemove(m_url);
                m_user = "guest";
            }
        }
    }

    bool changePassword = false;
    if(key == "password") {
        AuthStore::authRemove(m_url);
        changePassword = true;
    }

    CPLJSONDocument connectionFile;
    if(!connectionFile.Load(m_path)) {
        return false;
    }
    CPLJSONObject root = connectionFile.GetRoot();
    root.Set(KEY_URL, m_url);
    root.Set(KEY_LOGIN, m_user);
    if(changePassword) {
        if(!value.empty()) {
            root.Set(KEY_PASSWORD, encrypt(value));
        }
        else {
            root.Set(KEY_PASSWORD, value);
        }
    }

    root.Set(KEY_IS_GUEST, m_user == "guest");
    if(!connectionFile.Save(m_path)) {
        return false;
    }

    clear();

    return true;
}

}
