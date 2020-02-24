/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019-2020 NextGIS, <info@nextgis.com>
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
#include "ds/ngw.h"
#include "ds/simpledataset.h"
#include "file.h"
#include "ngstore/catalog/filter.h"
#include "util/account.h"
#include "util/authstore.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/stringutil.h"
#include "util/url.h"


namespace ngs {

constexpr const char *NGW_METADATA_DOMAIN = "ngw";

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

std::string NGWConnectionBase::userPwd() const
{
    if(compare(m_user, "guest")) {
        return "";
    }
    return m_user + ":" + m_password;
}

SpatialReferencePtr NGWConnectionBase::spatialReference() const
{
    SpatialReferencePtr out(new OGRSpatialReference);
    out->importFromEPSG(3857);
    return out;
}

//------------------------------------------------------------------------------
// NGWResourceBase
//------------------------------------------------------------------------------
static bool checkCanSync(const CPLJSONObject &resource)
{
    auto cls = resource.GetString("resource/cls");
    return compare(cls, "lookup_table");
}

NGWResourceBase::NGWResourceBase(const CPLJSONObject &resource,
                                 NGWConnectionBase *connection) :
    m_connection(connection)
{
    if(resource.IsValid()) {
        m_resourceId = resource.GetString("resource/id");
        m_creationDate = resource.GetString("resource/creation_date");
        m_keyName = resource.GetString("resource/keyname");
        m_description = resource.GetString("resource/description");
        auto metaItems = resource.GetObj("resource/resmeta/items");
        auto children = metaItems.GetChildren();
        for(const auto &child : children) {
            std::string osSuffix = ngw::resmetaSuffix( child.GetType() );
            m_resmeta[child.GetName() + osSuffix] = child.ToString();
        }

        m_canSync = checkCanSync(resource);
    }
    else {
        m_resourceId = "0";
    }
}

std::string NGWResourceBase::url() const
{
    if(m_connection) {
        return m_connection->connectionUrl();
    }
    return "";
}

Properties NGWResourceBase::metadata(const std::string &domain) const
{
    Properties out;
    if(domain.empty()) {
        out.add("url", url() + "/resource/" + m_resourceId);
        out.add("creation_date", m_creationDate);
        out.add("keyname", m_keyName);
        out.add("description", m_description);
        out.add("can_sync", m_canSync);
    }

    if(domain == NGW_METADATA_DOMAIN) {
        for(const auto &resmetaItem : m_resmeta) {
            out.add(resmetaItem.first, resmetaItem.second);
        }
    }
    return out;
}

std::string NGWResourceBase::metadataItem(const std::string &key,
                                          const std::string &defaultValue,
                                          const std::string &domain) const
{
    if(domain.empty()) {
        if(key == "url") {
            return url() + "/resource/" + m_resourceId;
        }
        else if(key == "id") {
            return m_resourceId;
        }
        else if(key == "creation_date") {
            return m_creationDate;
        }
        else if(key == "keyname") {
            return m_keyName;
        }
        else if(key == "description") {
            return m_description;
        }
        else if(key == "can_sync") {
            return m_canSync ? "YES" : "NO";
        }
    }

    if(domain == NGW_METADATA_DOMAIN) {
        if(m_resmeta.find(key) != m_resmeta.end()) {
            return m_resmeta.at(key);
        }
    }
    return defaultValue;
}

bool NGWResourceBase::canSync() const
{
    return m_canSync;
}

std::string NGWResourceBase::resourceId() const
{
    return m_resourceId;
}

NGWConnectionBase *NGWResourceBase::connection() const
{
    return m_connection;
}

bool NGWResourceBase::isNGWResource(const enum ngsCatalogObjectType type)
{
    return type >= CAT_NGW_ANY && type < CAT_NGW_ALL;
}

bool NGWResourceBase::remove()
{
    return ngw::deleteResource(url(), m_resourceId,
                               http::getGDALHeaders(url()).StealList());
}

bool NGWResourceBase::changeName(const std::string &newName)
{
    return ngw::renameResource(url(), m_resourceId, newName,
                               http::getGDALHeaders(url()).StealList());
}

//------------------------------------------------------------------------------
// NGWResource
//------------------------------------------------------------------------------

NGWResource::NGWResource(ObjectContainer * const parent,
                         const enum ngsCatalogObjectType type,
                         const std::string &name, const CPLJSONObject &resource,
                         NGWConnectionBase *connection) :
    Object(parent, type, name, ""),
    NGWResourceBase(resource, connection)
{
}

bool NGWResource::destroy()
{
    return NGWResourceBase::remove();
}

bool NGWResource::canDestroy() const
{
    return true; // Not check user rights here as server will report error if no access.
}

bool NGWResource::rename(const std::string &newName)
{
    return NGWResourceBase::changeName(newName);
}

bool NGWResource::canRename() const
{
    return true; // Not check user rights here as server will report error if no access.
}

Properties NGWResource::properties(const std::string &domain) const
{
    return metadata(domain);
}

std::string NGWResource::property(const std::string &key,
                                  const std::string &defaultValue,
                                  const std::string &domain) const
{
    return metadataItem(key, defaultValue, domain);
}

//------------------------------------------------------------------------------
// NGWResourceGroup
//------------------------------------------------------------------------------

NGWResourceGroup::NGWResourceGroup(ObjectContainer * const parent,
                                   const std::string &name,
                                   const CPLJSONObject &resource,
                                   NGWConnectionBase *connection) :
    ObjectContainer(parent, CAT_NGW_GROUP, name, ""), // Don't need file system path
    NGWResourceBase(resource, connection)
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

    if(cls == "resource_group") {
        addChild(ObjectPtr(new NGWResourceGroup(this, name, resource, m_connection)));
    }
    else if(cls == "trackers_group") {
        addChild(ObjectPtr(new NGWTrackersGroup(this, name, resource, m_connection)));
    }
    else if(cls == "trackers") {
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_TRACKER, name, resource, m_connection)));
    }
    else if(cls == "postgis_connection") {
        // TODO: List DB schemes and tables if client can connect to Postgres server
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_POSTGIS_CONNECTION, name, resource, m_connection)));
    }
    else if(cls == "wmsclient_connection") {
        // TODO: List WMS layers if client can connect to WMS server
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_WMS_CONNECTION, name, resource, m_connection)));
    }
    else if(cls == "vector_layer") {

        addChild(ObjectPtr(new NGWFeatureClass(this, CAT_NGW_VECTOR_LAYER, name, resource, m_connection)));
    }
    else if(cls == "postgis_layer") {

        addChild(ObjectPtr(new NGWFeatureClass(this, CAT_NGW_POSTGIS_LAYER, name, resource, m_connection)));
    }
    else if(cls == "raster_style") {
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_RASTER_STYLE, name, resource, m_connection)));
    }
    else if(cls == "basemap_layer") {
        // TODO: Same as raster style
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_BASEMAP, name, resource, m_connection)));
    }
    else if(cls == "wmsclient_layer") {
        // TODO: Same as raster style
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_WMS_LAYER, name, resource, m_connection)));
    }
    else if(cls == "raster_layer") {

//        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_RASTER_LAYER, name, resource, m_connection)));
    }
    else if(cls == "mapserver_style") {
        // TODO: Upload/download style
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_MAPSERVER_STYLE, name, resource, m_connection)));
    }
    else if(cls == "qgis_raster_style") {
        // TODO: Upload/download style
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_QGISRASTER_STYLE, name, resource, m_connection)));
    }
    else if(cls == "qgis_vector_style") {
        // TODO: Upload/download style
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_QGISVECTOR_STYLE, name, resource, m_connection)));
    }
    else if(cls == "formbuilder_form") {
        // TODO: Upload/download form
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_FORMBUILDER_FORM, name, resource, m_connection)));
    }
    else if(cls == "lookup_table") {
        // TODO: Download content for fields mapping. Change content. Sync content
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_LOOKUP_TABLE, name, resource, m_connection)));
    }
    else if(cls == "webmap") {
        // TODO: Add/change/remove groups and map layers/styles
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_WEBMAP, name, resource, m_connection)));
    }
    else if(cls == "file_bucket") {
        // TODO: Add/change/remove groups and files
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_FILE_BUCKET, name, resource, m_connection)));
    }
    else if(cls == "wmsserver_service") {
        // TODO: Add/change/remove layers
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_WMS_SERVICE, name, resource, m_connection)));
    }
    else if(cls == "wfsserver_service") {
        // TODO: Add/change/remove layers
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_WFS_SERVICE, name, resource, m_connection)));
    }
}

bool NGWResourceGroup::canCreate(const enum ngsCatalogObjectType type) const
{
    if(m_connection == nullptr) {
        return false; // Not expected that connection is null
    }
    return m_connection->isClsSupported(ngw::objectTypeToNGWClsType(type));
}

bool NGWResourceGroup::canDestroy() const
{
    return true; // Not check user rights here as server will report error if no access.
}

bool NGWResourceGroup::rename(const std::string &newName)
{
    return NGWResourceBase::changeName(newName);
}

bool NGWResourceGroup::canRename() const
{
    return true; // Not check user rights here as server will report error if no access.
}

Properties NGWResourceGroup::properties(const std::string &domain) const
{
    return metadata(domain);
}

std::string NGWResourceGroup::property(const std::string &key,
                                       const std::string &defaultValue,
                                       const std::string &domain) const
{
    return metadataItem(key, defaultValue, domain);
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

    child = ObjectPtr();
    if(type == CAT_NGW_VECTOR_LAYER) {
        Options newOptions =
                NGWFeatureClass::openOptions(m_connection->userPwd(), options);
        child = ObjectPtr(NGWFeatureClass::createFeatureClass(this, newName,
                                                              newOptions));
    }
    else {
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
            payload.Format(CPLJSONObject::Plain), http::getGDALHeaders(url()).StealList());
        if(compare(resourceId, "-1", true)) {
            return ObjectPtr();
        }

        resource.Add("id", atoi(resourceId.c_str()));

        if(m_childrenLoaded) {
            switch(type) {
            case CAT_NGW_GROUP:
                child = ObjectPtr(new NGWResourceGroup(this, newName, payload,
                                                       m_connection));
                break;
            case CAT_NGW_TRACKERGROUP:
                child = ObjectPtr(new NGWTrackersGroup(this, newName, payload,
                                                       m_connection));
                break;
            default:
                return ObjectPtr();
            }
        }
    }

    if(child == nullptr) {
        return child;
    }

    m_children.push_back(child);
    std::string newPath = fullName() + Catalog::separator() + newName;
    Notify::instance().onNotify(newPath, ngsChangeCode::CC_CREATE_OBJECT);

    return child;
}

bool NGWResourceGroup::canPaste(const enum ngsCatalogObjectType type) const
{
    return Filter::isFeatureClass(type);
}

int NGWResourceGroup::paste(ObjectPtr child, bool move, const Options &options,
                            const Progress &progress)
{
    std::string newName = options.asString("NEW_NAME",
                                           File::getBaseName(child->name()));
    newName = normalizeDatasetName(newName);
    if(newName.empty()) {
        errorMessage(_("Failed to create unique name."));
        return COD_LOAD_FAILED;
    }
    if(move) {
        progress.onProgress(COD_IN_PROCESS, 0.0,
                        _("Move '%s' to '%s'"), newName.c_str(), m_name.c_str());
    }
    else {
        progress.onProgress(COD_IN_PROCESS, 0.0,
                        _("Copy '%s' to '%s'"), newName.c_str(), m_name.c_str ());
    }

    if(child->type() == CAT_CONTAINER_SIMPLE) {
        SimpleDataset * const dataset = ngsDynamicCast(SimpleDataset, child);
        dataset->loadChildren();
        child = dataset->internalObject();
    }

    if(!child) {
        return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                          _("Source object is invalid"));
    }

    if(move && isNGWResource(child->type())) {
        NGWResourceBase * const base = ngsDynamicCast(NGWResourceBase, child);
        if(nullptr != base) {
            // Check move resource inside same NGW
            if(base->connection()->connectionUrl() ==
                    connection()->connectionUrl()) {
                // TODO: add move resources option
                // Remove child from original container add to this container
                return COD_SUCCESS;
            }
        }
    }

    if(Filter::isFeatureClass(child->type())) {
        FeatureClassPtr srcFClass = std::dynamic_pointer_cast<FeatureClass>(child);
        if(!srcFClass) {
            return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                _("Source object '%s' report type FEATURECLASS, but it is not a feature class"),
                child->name().c_str());
        }

        if(srcFClass->featureCount() > MAX_FEATURES4UNSUPPORTED) {
            const char *appName = CPLGetConfigOption("APP_NAME", "ngstore");
            if(!Account::instance().isFunctionAvailable(appName, "paste_features")) {
                return outMessage(COD_FUNCTION_NOT_AVAILABLE,
                    _("Cannot %s " CPL_FRMT_GIB " features on your plan, or account is not authorized"),
                    move ? _("move") : _("copy"), srcFClass->featureCount());
            }
        }

        bool toMulti = options.asBool("FORCE_GEOMETRY_TO_MULTI", false);
        OGRFeatureDefn * const srcDefinition = srcFClass->definition();
        bool ogrStyleField = options.asBool("OGR_STYLE_FIELD", false);
        if(ogrStyleField) {
            OGRFieldDefn ogrStyleField(OGR_STYLE_FIELD, OFTString);
            srcDefinition->AddFieldDefn(&ogrStyleField);
        }
        std::vector<OGRwkbGeometryType> geometryTypes =
                srcFClass->geometryTypes();
        OGRwkbGeometryType filterGeometryType =
                    FeatureClass::geometryTypeFromName(
                        options.asString("ACCEPT_GEOMETRY", "ANY"));
        Options createOptions(options);
        auto spatialRef = m_connection->spatialReference();
        for(OGRwkbGeometryType geometryType : geometryTypes) {
            if(filterGeometryType != geometryType &&
                    filterGeometryType != wkbUnknown) {
                continue;
            }
            std::string createName = newName;
            OGRwkbGeometryType newGeometryType = geometryType;
            if(geometryTypes.size() > 1 && filterGeometryType == wkbUnknown) {
                auto key = createOptions.asString("KEY");
                if(!key.empty()) {
                    warningMessage(_("The key metadata item set, but %d diffeerent layers will create. Omit key."),
                                   static_cast<int>(geometryTypes.size()));
                    createOptions.remove("KEY");
                }

                createName += "_";
                createName += FeatureClass::geometryTypeName(geometryType,
                                      FeatureClass::GeometryReportType::SIMPLE);
                if(toMulti && OGR_GT_Flatten(geometryType) < wkbMultiPoint) {
                    newGeometryType = static_cast<OGRwkbGeometryType>(geometryType + 3);
                }
            }

            auto userPwd = m_connection->userPwd();
            if(!userPwd.empty()) {
                createOptions.add("USERPWD", userPwd);
            }
            auto dstFClass = NGWFeatureClass::createFeatureClass(this, createName,
                srcDefinition, spatialRef, newGeometryType, createOptions);
            if(nullptr == dstFClass) {
                return move ? COD_MOVE_FAILED : COD_COPY_FAILED;
            }
            auto dstFields = dstFClass->fields();
            // Create fields map. We expected equal count of fields
            FieldMapPtr fieldMap(dstFields.size());
            for(int i = 0; i < static_cast<int>(dstFields.size()); ++i) {
                if(i == static_cast<int>(dstFields.size() - 1) && ogrStyleField) {
                    // Add OGR Style manualy
                    fieldMap[i] = -1;
                }
                else {
                    fieldMap[i] = i;
                }
            }

            Progress progressMulti(progress);
            progressMulti.setTotalSteps(2);
            progressMulti.setStep(0);

            int result = dstFClass->copyFeatures(srcFClass, fieldMap,
                                                 filterGeometryType,
                                                 progressMulti, createOptions);
            if(result != COD_SUCCESS) {
                return result;
            }

            progressMulti.setStep(1);
            auto fullNameStr = dstFClass->fullName();
            // Execute postprocess after features copied.
            if(!dstFClass->onRowsCopied(srcFClass, progressMulti, createOptions)) {
                warningMessage(_("Postprocess features after copy in feature class '%s' failed."),
                               fullNameStr.c_str());
            }

            addChild(ObjectPtr(dstFClass));
            Notify::instance().onNotify(fullNameStr, ngsChangeCode::CC_CREATE_OBJECT);
            progressMulti.onProgress(COD_FINISHED, 1.0, "");
        }
    }
    else {
        return outMessage(COD_UNSUPPORTED,
                          _("'%s' has unsuported type"), child->name().c_str());
    }

    if(move) {
        return child->destroy() ? COD_SUCCESS : COD_DELETE_FAILED;
    }
    return COD_SUCCESS;
}

std::string NGWResourceGroup::normalizeDatasetName(const std::string &name) const
{
    std::string outName;
    if(name.empty()) {
        outName = "new_dataset";
    }
    else {
        outName = name;
    }

    std::string originName = outName;
    int nameCounter = 0;
    while(!isNameValid(outName)) {
        outName = originName + "_" + std::to_string(++nameCounter);
        if(nameCounter == MAX_EQUAL_NAMES) {
            return "";
        }
    }

    return outName;
}

bool NGWResourceGroup::isNameValid(const std::string &name) const
{
    if(name.empty()) {
        return false;
    }

    for(const ObjectPtr &object : m_children) {
        if(compare(object->name(), name)) {
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// NGWTrackersGroup
//------------------------------------------------------------------------------

NGWTrackersGroup::NGWTrackersGroup(ObjectContainer * const parent,
                                   const std::string &name,
                                   const CPLJSONObject &resource,
                                   NGWConnectionBase *connection) :
    NGWResourceGroup(parent, name, resource, connection)
{
    m_type = CAT_NGW_TRACKERGROUP;
}

Properties NGWTrackersGroup::properties(const std::string &domain) const
{
    return metadata(domain);
}

std::string NGWTrackersGroup::property(const std::string &key,
                                       const std::string &defaultValue,
                                       const std::string &domain) const
{
    return metadataItem(key, defaultValue, domain);
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
        payload.Format(CPLJSONObject::Plain), http::getGDALHeaders(url()).StealList());
    if(compare(resourceId, "-1", true)) {
        return ObjectPtr();
    }

    resource.Add("id", atoi(resourceId.c_str()));

    if(m_childrenLoaded) {
        switch(type) {
        case CAT_NGW_TRACKER:
            child = ObjectPtr(new NGWResource(this, type, newName, payload, m_connection));
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
            if(searchReq.LoadUrl(m_searchApiUrl + "?serialization=full", http::getGDALHeaders(m_url))) {
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

    if(!isOpened()) {
        m_opened = true;
    }
    return true;
}

void NGWConnection::fillCapabilities()
{
    // Check NGW version. Paging available from 3.1
    CPLJSONDocument routeReq;
    if(routeReq.LoadUrl( ngw::getRouteUrl(m_url), http::getGDALHeaders(m_url) )) {
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
    if(schemaReq.LoadUrl( ngw::getSchemaUrl(m_url), http::getGDALHeaders(m_url) )) {
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
            m_url = root.GetString(URL_KEY);

            m_user = root.GetString(KEY_LOGIN);
            if(!m_user.empty() && !compare(m_user, "guest")) {
                std::string password = decrypt(root.GetString(KEY_PASSWORD));

                Options options;
                options.add("type", "basic");
                options.add("login", m_user);
                options.add("password", password);
                AuthStore::authAdd(m_url, options);

                m_password = password;
            }
        }
    }
}

Properties NGWConnection::properties(const std::string &domain) const
{
    fillProperties();
    Properties out = ObjectContainer::properties(domain);
    if(domain.empty()) {
        out.add("url", m_url);
        out.add("login", m_user);
        out.add("is_guest", compare(m_user, "guest") ? "yes" : "no");
        return out;
    }
    return out;
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

        if(key == "login") {
            return m_user;
        }

        if(key == "is_guest") {
            return compare(m_user, "guest") ? "yes" : "no";
        }
    }
    return ObjectContainer::property(key, defaultValue, domain);
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
        if(toBool(value)) {
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
    root.Set(URL_KEY, m_url);
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

    close();

    return true;
}

bool NGWConnection::open()
{
    if(isOpened()) {
        return true;
    }
    loadChildren();
    m_opened = true;
    return true;
}

void NGWConnection::close()
{
    clear();
    m_opened = false;
}

}
