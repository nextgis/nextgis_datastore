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
// NGWResourceBase
//------------------------------------------------------------------------------
NGWResourceBase::NGWResourceBase(const std::string &url,
                                 const std::string &resourceId) :
    m_url(url),
    m_resourceId(resourceId)
{

}

bool NGWResourceBase::remove()
{
    return ngw::deleteResource(m_url, m_resourceId,
                               getGDALHeaders(m_url).StealList());
}

//------------------------------------------------------------------------------
// NGWResource
//------------------------------------------------------------------------------
NGWResource::NGWResource(ObjectContainer * const parent,
                         const enum ngsCatalogObjectType type,
                         const std::string &name,
                         const std::string &url,
                         const std::string &resourceId) :
    Object(parent, type, name, ""),
    NGWResourceBase(url, resourceId)
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
                         const std::string &url,
                         const std::string &resourceId) :
    ObjectContainer(parent, CAT_CONTAINER_NGWGROUP, name, ""), // Don't need file system path
    NGWResourceBase(url, resourceId)
{

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
        addChild(ObjectPtr(new NGWResourceGroup(this, name, m_url, id)));
    }
    else if(cls == "trackers_group") {
        addChild(ObjectPtr(new NGWTrackersGroup(this, name, m_url, id)));
    }
}

bool NGWResourceGroup::canCreate(const enum ngsCatalogObjectType type) const
{
    switch (type) {
    case CAT_CONTAINER_NGWGROUP:
    case CAT_CONTAINER_NGWTRACKERGROUP:
        return true;
    default:
        return false;
    }
}

bool NGWResourceGroup::destroy()
{
    return NGWResourceBase::remove();
}

bool NGWResourceGroup::create(const enum ngsCatalogObjectType type,
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
                return errorMessage(_("Failed to overwrite %s\nError: %s"),
                                    newName.c_str(), getLastError());
            }
        }
        else {
            return errorMessage(_("Resource %s already exists. Add overwrite option or create_unique option to create resource here"),
                              newName.c_str());
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

    std::string resourceId = ngw::createResource(m_url,
        payload.Format(CPLJSONObject::Plain), getGDALHeaders(m_url).StealList());
    if(compare(resourceId, "-1", true)) {
        return false;
    }

    if(m_childrenLoaded) {
        switch(type) {
        case CAT_CONTAINER_NGWGROUP:
            m_children.push_back(ObjectPtr(new NGWResourceGroup(this, newName,
                                                                m_url, resourceId)));
            break;
        case CAT_CONTAINER_NGWTRACKERGROUP:
            m_children.push_back(ObjectPtr(new NGWTrackersGroup(this, newName,
                                                                m_url, resourceId)));
            break;
        default:
            return false;
        }
    }

    std::string newPath = fullName() + Catalog::separator() + newName;
    Notify::instance().onNotify(newPath, ngsChangeCode::CC_CREATE_OBJECT);

    return true;
}

//------------------------------------------------------------------------------
// NGWTrackersGroup
//------------------------------------------------------------------------------

NGWTrackersGroup::NGWTrackersGroup(ObjectContainer * const parent,
                         const std::string &name,
                         const std::string &url,
                         const std::string &resourceId) :
    NGWResourceGroup(parent, name, url, resourceId)
{
    m_type = CAT_CONTAINER_NGWTRACKERGROUP;
}

bool NGWTrackersGroup::canCreate(const enum ngsCatalogObjectType type) const
{
    switch (type) {
    case CAT_NGW_TRACKER:
        return true;
    default:
        return false;
    }
}

bool NGWTrackersGroup::create(const enum ngsCatalogObjectType type,
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
                return errorMessage(_("Failed to overwrite %s\nError: %s"),
                                    newName.c_str(), getLastError());
            }
        }
        else {
            return errorMessage(_("Resource %s already exists. Add overwrite option or create_unique option to create resource here"),
                              newName.c_str());
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

    std::string resourceId = ngw::createResource(m_url,
        payload.Format(CPLJSONObject::Plain), getGDALHeaders(m_url).StealList());
    if(compare(resourceId, "-1", true)) {
        return false;
    }

    if(m_childrenLoaded) {
        switch(type) {
        case CAT_NGW_TRACKER:
            m_children.push_back(ObjectPtr(new NGWResource(this, type, newName,
                                                           m_url, resourceId)));
            break;
        default:
            return false;
        }
    }

    std::string newPath = fullName() + Catalog::separator() + newName;
    Notify::instance().onNotify(newPath, ngsChangeCode::CC_CREATE_OBJECT);

    return true;
}

//------------------------------------------------------------------------------
// NGWConnection
//------------------------------------------------------------------------------

NGWConnection::NGWConnection(ObjectContainer * const parent,
                         const std::string &name,
                         const std::string &path) :
    NGWResourceGroup(parent, name, "")
{
    m_type = CAT_CONTAINER_NGW;
    m_path = path;
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

    CPLJSONDocument connectionFile;
    if(connectionFile.Load(m_path)) {
        CPLJSONObject root = connectionFile.GetRoot();
        m_url = root.GetString(KEY_URL);

        std::string user = root.GetString(KEY_LOGIN);
        if(!user.empty() && !compare(user, "guest")) {
            std::string password = decrypt(root.GetString(KEY_PASSWORD));

            Options options;
            options.add("type", "basic");
            options.add("login", user);
            options.add("password", password);
            AuthStore::authAdd(m_url, options);
        }
    }

    if(!m_url.empty()) {
        fillCapabilities();
        if(!m_searchApiUrl.empty()) {
            CPLJSONDocument searchReq;
            if(searchReq.LoadUrl(m_searchApiUrl + "?serialization=full",
                                  getGDALHeaders(m_url))) {
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

}
