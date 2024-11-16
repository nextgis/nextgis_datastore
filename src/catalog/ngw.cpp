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
#include "ds/util.h"
#include "file.h"
#include "ngstore/catalog/filter.h"
#include "util/account.h"
#include "util/authstore.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/settings.h"
#include "util/stringutil.h"
#include "util/url.h"


namespace ngs {

constexpr const char *NGW_METADATA_DOMAIN = "ngw";

static void addResourceInt(ObjectPtr parent, CPLJSONObject resource)
{
    auto group = ngsDynamicCast(NGWResourceGroup, parent);
    if(group) {
        group->addResource(resource);
        return;
    }

    auto layer = ngsDynamicCast(NGWLayerDataset, parent);
    if(layer) {
        layer->addResource(resource);
        return;
    }

    auto raster = ngsDynamicCast(NGWRasterDataset, parent);
    if(raster) {
        raster->addResource(resource);
        return;
    }
}

static CPLJSONObject createResourcePayload(NGWResourceBase *parent,
                                    const enum ngsCatalogObjectType type,
                                    const std::string &name,
                                    const Options &options)
{
    CPLJSONObject payload;
    CPLJSONObject resource("resource", payload);
    resource.Add("cls", ngw::objectTypeToNGWClsType(type));
    resource.Add("display_name", name);
    std::string key = options.asString("KEY");
    if(!key.empty()) {
        resource.Add("keyname", key);
    }
    std::string desc = options.asString("DESCRIPTION");
    if(!desc.empty()) {
        resource.Add("description", desc);
    }
    CPLJSONObject parentResource("parent", resource);
    parentResource.Add("id", atoi(parent->resourceId().c_str()));

    return payload;
}

static bool checkIsSyncable(const CPLJSONObject &resource)
{
    // TODO: Add vector layer support
    auto cls = resource.GetString("resource/cls");
    return compare(cls, "lookup_table");
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

std::string NGWConnectionBase::userPwd() const
{
    if(m_isGuest || compare(m_user, "guest")) {
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

NGWResourceBase::NGWResourceBase(const CPLJSONObject &resource,
                                 NGWConnectionBase *connection) :
    m_resourceId("0"),
    m_connection(connection)
{
    if(resource.IsValid()) {
        m_resourceId = resource.GetString("resource/id", "0");
        m_creationDate = resource.GetString("resource/creation_date");
        m_keyName = resource.GetString("resource/keyname");
        m_description = resource.GetString("resource/description");
        auto metaItems = resource.GetObj("resmeta/items");
        auto children = metaItems.GetChildren();
        for(const auto &child : children) {
            std::string osSuffix = ngw::resmetaSuffix( child.GetType() );
            m_resmeta[child.GetName() + osSuffix] = child.ToString();
        }

        m_isSyncable = checkIsSyncable(resource);
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
        out.add("id", m_resourceId);
        out.add("creation_date", m_creationDate);
        out.add("keyname", m_keyName);
        out.add("description", m_description);
        out.add("is_syncable", m_isSyncable);
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
        if(compare(key, "url")) {
            return url() + "/resource/" + m_resourceId;
        }
        else if(compare(key, "id")) {
            return m_resourceId;
        }
        else if(compare(key, "creation_date")) {
            return m_creationDate;
        }
        else if(compare(key, "keyname")) {
            return m_keyName;
        }
        else if(compare(key, "description")) {
            return m_description;
        }
        else if(compare(key, "is_syncable")) {
            return fromBool(m_isSyncable);
        }
    }

    if(domain == NGW_METADATA_DOMAIN) {
        if(m_resmeta.find(key) != m_resmeta.end()) {
            return m_resmeta.at(key);
        }
    }
    return defaultValue;
}

bool NGWResourceBase::isSyncable() const
{
    return m_isSyncable;
}

CPLJSONObject NGWResourceBase::asJson() const
{
    CPLJSONObject payload;
    CPLJSONObject resource("resource", payload);
    if(!m_keyName.empty()) {
        resource.Add("keyname", m_keyName);
    }
    if(!m_description.empty()) {
        resource.Add("description", m_description);
    }

    if(!m_resmeta.empty()) {
        CPLJSONObject resMeta("resmeta", payload);
        CPLJSONObject resMetaItems("items", resMeta);
        for( const auto &remetaItem : m_resmeta ) {
            auto itemName = remetaItem.first;
            auto itemValue = remetaItem.second;
            auto suffixPos = itemName.size() - 2;
            auto suffix = itemName.substr(suffixPos);
            if( suffix == ".d") {
                resMetaItems.Add( itemName.substr(0, suffixPos),
                                  CPLAtoGIntBig( itemValue.c_str() ) );
                continue;
            }
            if( suffix == ".f") {
                resMetaItems.Add( itemName.substr(0, suffixPos),
                                  CPLAtofM(itemValue.c_str() ) );
                continue;
            }
            resMetaItems.Add( itemName, itemValue );
        }
    }

    return payload;
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
    NGWResourceBase(resource, connection),
    m_hasPendingChanges(false)
{
}

NGWResource::~NGWResource()
{
    if(m_hasPendingChanges) {
        sync();
    }
}

bool NGWResource::destroy()
{
    if(!NGWResourceBase::remove()) {
        return false;
    }

    return Object::destroy();
}

bool NGWResource::canDestroy() const
{
    return true; // Not check user rights here as server will report error if no access.
}

bool NGWResource::rename(const std::string &newName)
{
    if(NGWResourceBase::changeName(newName)) {
        m_name = newName;
        return true;
    }
    return false;
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

bool NGWResource::sync()
{
    auto result = ngw::updateResource(url(), resourceId(),
                                      asJson().Format(CPLJSONObject::PrettyFormat::Plain),
                                      http::getGDALHeaders(url()).StealList());
    if(result) {
        m_hasPendingChanges = false;
    }
    return result;
}


CPLJSONObject NGWResource::asJson() const
{
    auto payload = NGWResourceBase::asJson();
    CPLJSONObject resource = payload.GetObj("resource");
    resource.Add("display_name", m_name);

    auto ngwParentResource = dynamic_cast<NGWResourceBase*>(m_parent);
    if(ngwParentResource) {
        CPLJSONObject parent("parent", resource);
        parent.Add("id", atoi(ngwParentResource->resourceId().c_str()));
    }

//    resource.Add("cls", ngw::objectTypeToNGWClsType(m_type));
//    resource.Add("id", atoi(m_resourceId.c_str()));

    return payload;
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
        auto group = ngsDynamicCast(NGWResourceGroup, child);
        if(group != nullptr) {
            ObjectPtr resource = group->getResource(resourceId);
            if(resource) {
                return resource;
            }
        }

        auto layer = ngsDynamicCast(NGWLayerDataset, child);
        if(layer != nullptr) {
            ObjectPtr resource = layer->getResource(resourceId);
            if(resource) {
                return resource;
            }
        }

        auto raster = ngsDynamicCast(NGWRasterDataset, child);
        if(raster != nullptr) {
            ObjectPtr resource = raster->getResource(resourceId);
            if(resource) {
                return resource;
            }
        }

        auto resource = ngsDynamicCast(NGWResourceBase, child);
        if(resource && resource->resourceId() == resourceId) {
            return child;
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

        addChild(ObjectPtr(new NGWLayerDataset(this, CAT_NGW_VECTOR_LAYER, name, resource, m_connection)));
    }
    else if(cls == "postgis_layer") {

        addChild(ObjectPtr(new NGWLayerDataset(this, CAT_NGW_POSTGIS_LAYER, name, resource, m_connection)));
    }
    else if(cls == "raster_style") {
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_RASTER_STYLE, name, resource, m_connection)));
    }
    else if(cls == "basemap_layer") {
        addChild(ObjectPtr(new NGWBaseMap(this, name, resource, m_connection)));
    }
    else if(cls == "wmsclient_layer") {
        // TODO: Same as raster style
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_WMS_LAYER, name, resource, m_connection)));
    }
    else if(cls == "raster_layer") {
        addChild(ObjectPtr(new NGWRasterDataset(this, name, resource, m_connection)));
    }
    else if(cls == "mapserver_style") {
        addChild(ObjectPtr(new NGWStyle(this, CAT_NGW_MAPSERVER_STYLE, name, resource, m_connection)));
    }
    else if(cls == "qgis_raster_style") {
        addChild(ObjectPtr(new NGWStyle(this, CAT_NGW_QGISRASTER_STYLE, name, resource, m_connection)));
    }
    else if(cls == "qgis_vector_style") {
        addChild(ObjectPtr(new NGWStyle(this, CAT_NGW_QGISVECTOR_STYLE, name, resource, m_connection)));
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
        addChild(ObjectPtr(new NGWWebMap(this, name, resource, m_connection)));
    }
    else if(cls == "file_bucket") {
        // TODO: Add/change/remove groups and files
        addChild(ObjectPtr(new NGWResource(this, CAT_NGW_FILE_BUCKET, name, resource, m_connection)));
    }
    else if(cls == "wmsserver_service") {
        addChild(ObjectPtr(new NGWService(this, CAT_NGW_WMS_SERVICE, name, resource, m_connection)));
    }
    else if(cls == "wfsserver_service") {
        addChild(ObjectPtr(new NGWService(this, CAT_NGW_WFS_SERVICE, name, resource, m_connection)));
    }
}

bool NGWResourceGroup::canCreate(const enum ngsCatalogObjectType type) const
{
    if(m_connection == nullptr || type == CAT_NGW_TRACKER ||
            type == CAT_NGW_QGISRASTER_STYLE || type == CAT_NGW_QGISVECTOR_STYLE ||
            type == CAT_NGW_MAPSERVER_STYLE || type == CAT_NGW_RASTER_STYLE ||
            type == CAT_NGW_WMS_LAYER || type == CAT_NGW_TRACKER ||
            type == CAT_NGW_FORMBUILDER_FORM) {
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
    if(NGWResourceBase::changeName(newName)) {
        m_name = newName;
        return true;
    }
    return false;
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
    if(!NGWResourceBase::remove()) {
        return false;
    }

    return ObjectContainer::destroy();
}

ObjectPtr NGWResourceGroup::create(const enum ngsCatalogObjectType type,
                                const std::string &name, const Options &options)
{
    loadChildren();

    std::string newName = name;
    if(options.asBool("CREATE_UNIQUE", false)) {
        newName = createUniqueName(newName, false);
    }

    ObjectPtr childPtr = getChild(newName);
    if(childPtr) {
        if(options.asBool("OVERWRITE", false)) {
            if(!childPtr->destroy()) {
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
    childPtr = nullptr;

    Object *child = nullptr;
    if(type == CAT_NGW_VECTOR_LAYER) {
        child = NGWLayerDataset::createFeatureClass(this, newName, options);
    }
    else if(type == CAT_NGW_BASEMAP) {
        child = NGWBaseMap::create(this, newName, options);
    }
    else if(type == CAT_NGW_WEBMAP) {
        child = NGWWebMap::create(this, newName, options);
    }
    else {        
        CPLJSONObject payload = createResourcePayload(this, type, name, options);
        std::string resourceId = ngw::createResource(url(),
            payload.Format(CPLJSONObject::PrettyFormat::Plain), http::getGDALHeaders(url()).StealList());
        if(compare(resourceId, "-1", true)) {
            return ObjectPtr();
        }

        payload.Add("resource/id", atoi(resourceId.c_str()));

        if(m_childrenLoaded) {
            switch(type) {
            case CAT_NGW_GROUP:
                child = new NGWResourceGroup(this, newName, payload, m_connection);
                break;
            case CAT_NGW_TRACKERGROUP:
                child = new NGWTrackersGroup(this, newName, payload, m_connection);
                break;
            case CAT_NGW_WEBMAP:
                child = new NGWWebMap(this, newName, payload, m_connection);
                break;
            case CAT_NGW_WFS_SERVICE:
            case CAT_NGW_WMS_SERVICE:
                child = new NGWService(this, type, newName, payload, m_connection);
                break;
            default:
                return ObjectPtr();
            }
        }
    }

    return onChildCreated(child);
}

bool NGWResourceGroup::canPaste(const enum ngsCatalogObjectType type) const
{
    return Filter::isFeatureClass(type) || Filter::isRaster(type);
}

int NGWResourceGroup::paste(ObjectPtr child, bool move, const Options &options,
                            const Progress &progress)
{
    resetError();
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
        auto dataset = ngsDynamicCast(SingleLayerDataset, child);
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
        std::shared_ptr<OGRFeatureDefn> srcDefinition(srcFClass->definition()->Clone());
        bool ogrStyleField = options.asBool("OGR_STYLE_STRING_TO_FIELD", false);
        if(ogrStyleField) {
            if(srcDefinition->GetFieldIndex(OGR_STYLE_FIELD) == -1) {
                OGRFieldDefn ogrStyleField(OGR_STYLE_FIELD, OFTString);
                srcDefinition->AddFieldDefn(&ogrStyleField);
            }
            else {
                warningMessage(_("The field %s already exists. All values will rewrite by OGR_STYLE. To prevent this remove OGR_STYLE_STRING_TO_FIELD option."), OGR_STYLE_FIELD);
            }
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
                    warningMessage(_("The key metadata item set, but %d different layers will create. Omit key."),
                                   static_cast<int>(geometryTypes.size()));
                    createOptions.remove("KEY");
                }

                createName += "_";
                createName += FeatureClass::geometryTypeName(geometryType,
                                      FeatureClass::GeometryReportType::SIMPLE);
            }

            if(toMulti && OGR_GT_Flatten(geometryType) <= wkbPolygon) {
                newGeometryType = static_cast<OGRwkbGeometryType>(geometryType + 3);
            }

            auto userPwd = m_connection->userPwd();
            if(!userPwd.empty()) {
                createOptions.add("USERPWD", userPwd);
            }
            auto dstDS = NGWLayerDataset::createFeatureClass(this, createName,
                srcDefinition.get(), spatialRef, newGeometryType, createOptions);
            if(nullptr == dstDS) {
                return move ? COD_MOVE_FAILED : COD_COPY_FAILED;
            }
            auto dstFClass = ngsDynamicCast(NGWFeatureClass, dstDS->internalObject());
            if(nullptr == dstFClass) {
                delete dstDS;
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
                                                 progressMulti, createOptions);
            if(result != COD_SUCCESS) {
                delete dstDS;
                return result;
            }

            progressMulti.setStep(1);
            auto fullNameStr = dstFClass->fullName();
            if(!dstFClass->sync()) {
                warningMessage(_("Sync of feature class '%s' failed."),
                               fullNameStr.c_str());
            }
            // Execute postprocess after features copied.
            if(!dstFClass->onRowsCopied(srcFClass, progressMulti, createOptions)) {
                warningMessage(_("Postprocess features after copy in feature class '%s' failed."),
                               fullNameStr.c_str());
            }

            onChildCreated(dstDS);
            progressMulti.onProgress(COD_FINISHED, 1.0, "");
        }
    }
    else if(Filter::isRaster(child->type())) {
        RasterPtr srcRaster = std::dynamic_pointer_cast<Raster>(child);
        if(!srcRaster) {
            return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                _("Source object '%s' report type RASTER, but it is not a raster"),
                child->name().c_str());
        }

        // Check available paste rasters
        if(srcRaster->width() > MAX_RASTERSIZE4UNSUPPORTED ||
                srcRaster->height() > MAX_RASTERSIZE4UNSUPPORTED) {
            const char *appName = CPLGetConfigOption("APP_NAME", "ngstore");
            if(!Account::instance().isFunctionAvailable(appName, "paste_raster")) {
                return outMessage(COD_FUNCTION_NOT_AVAILABLE,
                    _("Cannot %s raster on your plan, or account is not authorized"),
                    move ? _("move") : _("copy"));
            }
        }

        std::string rasterPath = child->path();

        Progress progressMulti(progress);
        progressMulti.setTotalSteps(2);
        progressMulti.setStep(0);

        // Check if source GDALDataset is tiff.
        if(srcRaster->type() != CAT_RASTER_TIFF ) {
            Settings &settings = Settings::instance();
            auto tmpPath = settings.getString("common/cache_path", "");
            if(tmpPath.empty()) {
                return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                                  _("Cache path option must be present"));
            }
            auto tmpName = random(10) + "." + Filter::extension(CAT_RASTER_TIFF);
            rasterPath = File::formFileName(tmpPath, tmpName);

            auto catalog = Catalog::instance();
            auto tempFolder = catalog->getObjectBySystemPath(tmpPath);
            auto tempObjectContainer = ngsDynamicCast(ObjectContainer, tempFolder);
            if(nullptr == tempObjectContainer) {
                return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                                  _("Cannot %s raster. Temp path not defined."),
                                  move ? _("move") : _("copy"));
            }

            Options options;
            options.add("TYPE", static_cast<long>(CAT_RASTER_TIFF));
            options.add("COMPRESS", "LZW");
            options.add("NUM_THREADS", static_cast<long>(getNumberThreads()));
            options.add("NEW_NAME", tmpName);

            int result = tempObjectContainer->paste(child, false, options,
                                                    progressMulti);
            if(result != COD_SUCCESS) {
                return result;
            }
        }

        progressMulti.setStep(1);

        auto uploadInfo = http::uploadFile(ngw::getUploadUrl(url()), rasterPath,
                                           progressMulti);
        auto uploadMetaArray = uploadInfo.GetArray("upload_meta");

        if(!compare(child->path(), rasterPath)) {
            File::deleteFile(rasterPath);
        }
        if(uploadMetaArray.Size() == 0) {
            return move ? COD_MOVE_FAILED : COD_COPY_FAILED;
        }

        CPLJSONObject payload;
        CPLJSONObject resource("resource", payload);
        resource.Add("cls", ngw::objectTypeToNGWClsType(CAT_NGW_RASTER_LAYER));
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

        CPLJSONObject raster("raster_layer", payload);
        raster.Add("source", uploadMetaArray[0]);
        CPLJSONObject srs("srs", raster);
        srs.Add( "id", 3857 );


        std::string resourceId = ngw::createResource(url(),
            payload.Format(CPLJSONObject::PrettyFormat::Plain), http::getGDALHeaders(url()).StealList());
        if(compare(resourceId, "-1", true)) {
            return move ? COD_MOVE_FAILED : COD_COPY_FAILED;
        }

        resource.Add("id", atoi(resourceId.c_str()));

        if(m_childrenLoaded) {
            onChildCreated(new NGWRasterDataset(this, newName, payload, m_connection));
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

bool NGWTrackersGroup::canCreate(const enum ngsCatalogObjectType type) const
{
    if(type == CAT_NGW_TRACKER && m_connection) {
        return m_connection->isClsSupported(ngw::objectTypeToNGWClsType(type));
    }
    return false;
}

ObjectPtr NGWTrackersGroup::create(const enum ngsCatalogObjectType type,
                    const std::string &name, const Options &options)
{
    loadChildren();

    std::string newName = name;
    if(options.asBool("CREATE_UNIQUE", false)) {
        newName = createUniqueName(newName, false);
    }

    auto child = getChild(newName);
    if(child) {
        if(options.asBool("OVERWRITE", false)) {
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
    child = nullptr;

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

    tracker.Add("is_registered", nullptr);

    std::string resourceId = ngw::createResource(url(),
        payload.Format(CPLJSONObject::PrettyFormat::Plain), http::getGDALHeaders(url()).StealList());
    if(compare(resourceId, "-1", true)) {
        return ObjectPtr();
    }

    resource.Add("id", atoi(resourceId.c_str()));

    switch(type) {
    case CAT_NGW_TRACKER:
    {
        auto child = new NGWResource(this, type, newName, payload, m_connection);
        return onChildCreated(child);
    }
    default:
        return ObjectPtr();
    }
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
            if(searchReq.LoadUrl(m_searchApiUrl, http::getGDALHeaders(m_url))) {
                CPLJSONArray root(searchReq.GetRoot());
                if(root.IsValid()) {
                    m_childrenLoaded = true;
                    std::vector<std::pair<std::string, CPLJSONObject>> noParentResources;
                    for(int i = 0; i < root.Size(); ++i) {
                        CPLJSONObject resource = root[i];
                        std::string resourceId =
                                resource.GetString("resource/parent/id", "-1");
                        if(resourceId != "-1") {
                            auto parent = getResource(resourceId);
                            if(parent) {
                                addResourceInt(parent, resource);
                            }
                            else {
                                noParentResources.push_back(std::make_pair(resourceId, resource));
                            }
                        }
                    }

                    int counter = 15;
                    while(!noParentResources.empty()) {
                        CPLDebug("--", "counter %d, size: %ld", counter, noParentResources.size());
                        auto it = noParentResources.begin();
                        while(it != noParentResources.end()) {
                            ObjectPtr parent;
                            if((parent = getResource(it->first))) {
                                addResourceInt(parent, it->second);
                                it = noParentResources.erase(it);
                            }
                            else {
                                ++it;
                            }
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
        return false;
    }

    return ObjectContainer::destroy();
}

void NGWConnection::fillProperties() const
{
    if(!m_opened || m_url.empty() || m_user.empty()) {
        CPLJSONDocument connectionFile;
        if(connectionFile.Load(m_path)) {
            CPLJSONObject root = connectionFile.GetRoot();
            m_url = root.GetString(URL_KEY);

            m_user = root.GetString(KEY_LOGIN);
            m_isGuest = root.GetBool(KEY_IS_GUEST);
            if(m_isGuest) {
                return;
            }

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
    Properties out = ObjectContainer::properties(domain);
    if(domain.empty()) {
        fillProperties();
        out.add("url", m_url);
        out.add("login", m_user);
        out.add("is_guest", fromBool(m_isGuest || compare(m_user, "guest")));
        out.append(NGWResourceBase::metadata(domain));
        return out;
    }
    return out;
}

std::string NGWConnection::property(const std::string &key,
                             const std::string &defaultValue,
                             const std::string &domain) const
{
    if(domain.empty()) {
        fillProperties();
        if(compare(key, "url")) {
            return m_url;
        }

        if(compare(key, "login")) {
            return m_user;
        }

        if(compare(key, "is_guest")) {
            return fromBool(m_isGuest || compare(m_user, "guest"));
        }

        auto out = NGWResourceBase::metadataItem(key, defaultValue, domain);
        if(out != defaultValue) {
            return out;
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

    fillProperties();

    if(compare(key, "url")) {
        m_url = value;
    }

    if(compare(key, "login")) {
        m_user = value;
    }

    if(compare(key, "is_guest")) {
        m_isGuest = toBool(value);
    }

    if(compare(key, "password")) {
        m_password = value;
    }
    AuthStore::authRemove(m_url);

    CPLJSONDocument connectionFile;
    if(!connectionFile.Load(m_path)) {
        return false;
    }

    if(compare(m_user, "guest")) {
        m_isGuest = true;
    }
    auto root = connectionFile.GetRoot();
    root.Set(URL_KEY, m_url);
    root.Set(KEY_LOGIN, m_user);
    if(!m_password.empty()) {
        root.Set(KEY_PASSWORD, encrypt(m_password));
    }
    else {
        root.Set(KEY_PASSWORD, m_password);
    }

    root.Set(KEY_IS_GUEST, m_isGuest);
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
    m_url.clear();
    m_user.clear();
    m_password.clear();
    m_isGuest = true;
}

//------------------------------------------------------------------------------
// NGWService
//------------------------------------------------------------------------------

//"wfsserver_service": {
//    "layers": [
//        {
//            "maxfeatures": 1000,
//            "keyname": "ngw_id_1733",
//            "display_name": "Eat here",
//            "resource_id": 1733
//        }
//    ]
//}

//"wmsserver_service": {
//    "layers": [
//        {
//            "min_scale_denom": null,
//            "keyname": "TrGIS",
//            "display_name": "TrGIS Horizontal wells",
//            "max_scale_denom": null,
//            "resource_id": 4241
//        }
//    ]
//}

NGWService::NGWService(ObjectContainer * const parent,
                       const enum ngsCatalogObjectType type,
                       const std::string &name,
                       const CPLJSONObject &resource,
                       NGWConnectionBase *connection) :
    NGWResource(parent, type, name, resource, connection)
{
    if(type == CAT_NGW_WFS_SERVICE) {
        auto layers = resource.GetArray("wfsserver_service/layers");
        for(int i = 0; i < layers.Size(); ++i) {
            auto layer = layers[i];
            auto ngwLayer = new NGWWFSServiceLayer(
                        layer.GetString("keyname"),
                        layer.GetString("display_name"),
                        layer.GetInteger("resource_id"),
                        layer.GetInteger("maxfeatures"));
            m_layers.push_back(NGWServiceLayerPtr(ngwLayer));
        }
    }
    else if(type == CAT_NGW_WMS_SERVICE) {
        auto layers = resource.GetArray("wmsserver_service/layers");
        for(int i = 0; i < layers.Size(); ++i) {
            auto layer = layers[i];
            auto ngwLayer = new NGWWMSServiceLayer(
                        layer.GetString("keyname"),
                        layer.GetString("display_name"),
                        layer.GetInteger("resource_id"),
                        layer.GetString("min_scale_denom"),
                        layer.GetString("max_scale_denom"));

            m_layers.push_back(NGWServiceLayerPtr(ngwLayer));
        }
    }
}

std::vector<NGWServiceLayerPtr> NGWService::layers() const
{
    return m_layers;
}

static bool isResourceTypeStyle(NGWResourceBase *resource) {
    auto object = dynamic_cast<Object*>(resource);
    if(nullptr == object) {
        return false;
    }

    return  object->type() == CAT_NGW_RASTER_STYLE ||
            object->type() == CAT_NGW_QGISRASTER_STYLE ||
            object->type() == CAT_NGW_QGISVECTOR_STYLE ||
            object->type() == CAT_NGW_MAPSERVER_STYLE ||
            object->type() == CAT_NGW_WMS_LAYER;
}

static bool isResourceTypeLayer(NGWResourceBase *resource) {
    auto object = dynamic_cast<Object*>(resource);
    if(nullptr == object) {
        return false;
    }

    if(object->type() == CAT_CONTAINER_SIMPLE) {
        auto simpleDataset = dynamic_cast<SingleLayerDataset*>(resource);
        if(nullptr == simpleDataset) {
            return false;
        }
        object = simpleDataset->internalObject().get();
    }

    return object && (object->type() == CAT_NGW_VECTOR_LAYER ||
           object->type() == CAT_NGW_POSTGIS_LAYER);
}

static bool isResourceTypeForService(enum ngsCatalogObjectType type,
                                     NGWResourceBase *resource) {
    if(type == CAT_NGW_WFS_SERVICE && !isResourceTypeLayer(resource)) {
        return errorMessage("Unsupported layer source. Expected vector layer or PostGIS layer, got %d",
                            type);
    }

    if(type == CAT_NGW_WMS_SERVICE &&
            !isResourceTypeStyle(resource)) {
        return errorMessage("Unsupported layer source. Expected style or WMS layer, got %d",
                            type);
    }
    return true;
}

static bool isKeyUnique(const std::string &key,
                        std::vector<NGWServiceLayerPtr> &layers) {
    for(const auto &layer : layers) {
        if(compare(layer->m_key, key)) {
            return false;
        }
    }
    return true;
}

bool NGWService::addLayer(const std::string &key, const std::string &name,
                          NGWResourceBase *resource)
{
    if(nullptr == resource) {
        return false;
    }

    if(!isResourceTypeForService(m_type, resource)) {
        return false;
    }

    if(!isKeyUnique(key, m_layers)) {
        return errorMessage(_("Key %s is not unique"), key.c_str());
    }

    NGWServiceLayerPtr newItem;
    if(m_type == CAT_NGW_WFS_SERVICE) {
        newItem = NGWServiceLayerPtr(new NGWWFSServiceLayer(key, name, resource));
    }
    else if(m_type == CAT_NGW_WMS_SERVICE) {
        newItem = NGWServiceLayerPtr(new NGWWMSServiceLayer(key, name, resource));
    }

    m_layers.push_back(newItem);

    m_hasPendingChanges = true;
    return true;
}

bool NGWService::changeLayer(const std::string &oldKey, const std::string &key,
                             const std::string &name, NGWResourceBase *resource)
{
    if(nullptr == resource) {
        return errorMessage(_("Resource pointer is empty."));
    }

    if(!isResourceTypeForService(m_type, resource)) {
        return false;
    }

    if(oldKey != key) {
        if(!isKeyUnique(key, m_layers)) {
            return errorMessage(_("Key %s is not unique"), key.c_str());
        }
    }

    auto it = m_layers.begin();
    while(it != m_layers.end()) {
        if(it->get()->m_key == oldKey) {
            it->get()->m_key = key;
            it->get()->m_name = name;
            it->get()->m_resourceId = atoi(resource->resourceId().c_str());

            m_hasPendingChanges = true;
            return true;
        }
        ++it;
    }

    return false;
}

bool NGWService::deleteLayer(const std::string &key)
{
    auto it = m_layers.begin();
    while(it != m_layers.end()) {
        if(it->get()->m_key == key) {
            m_layers.erase(it);

            m_hasPendingChanges = true;
            return true;
        }
        ++it;
    }
    return false;
}

CPLJSONObject NGWService::asJson() const
{
    auto payload = NGWResource::asJson();

    std::string serviceKeyName;
    if(m_type == CAT_NGW_WMS_SERVICE) {
        serviceKeyName = "wmsserver_service";
    }
    else if(m_type == CAT_NGW_WFS_SERVICE) {
        serviceKeyName = "wfsserver_service";
    }
    else {
        return CPLJSONObject();
    }
    CPLJSONObject service(serviceKeyName, payload);
    CPLJSONArray layers;
    for(const auto &layer : m_layers) {
        CPLJSONObject layerJson;
        layerJson.Add("keyname", layer->m_key);
        layerJson.Add("display_name", layer->m_name);
        layerJson.Add("resource_id", layer->m_resourceId);
        if(m_type == CAT_NGW_WMS_SERVICE) {
            auto wmsLayer = dynamic_cast<NGWWMSServiceLayer*>(layer.get());
            if(wmsLayer->m_minScaleDenom.empty()) {
                layerJson.AddNull("min_scale_denom");
            }
            else {
                layerJson.Add("min_scale_denom", wmsLayer->m_minScaleDenom);
            }

            if(wmsLayer->m_maxScaleDenom.empty()) {
                layerJson.AddNull("max_scale_denom");
            }
            else {
                layerJson.Add("max_scale_denom", wmsLayer->m_maxScaleDenom);
            }
        }
        else if(m_type == CAT_NGW_WFS_SERVICE) {
            auto wfsLayer = dynamic_cast<NGWWFSServiceLayer*>(layer.get());
            layerJson.Add("maxfeatures", wfsLayer->m_maxfeatures);
        }
        layers.Add(layerJson);
    }
    service.Add("layers", layers);
    return payload;
}

//------------------------------------------------------------------------------
// NGWServiceLayer
//------------------------------------------------------------------------------
NGWServiceLayer::NGWServiceLayer(const std::string &key, const std::string &name,
                                 NGWResourceBase *resource) :
    m_key(key),
    m_name(name),
    m_resourceId(-1)
{
    if(nullptr != resource) {
        m_resourceId = atoi(resource->resourceId().c_str());
    }
}

NGWServiceLayer::NGWServiceLayer(const std::string &key, const std::string &name,
                                 int resourceId) :
    m_key(key),
    m_name(name),
    m_resourceId(resourceId)
{

}

//------------------------------------------------------------------------------
// NGWWFSServiceLayer
//------------------------------------------------------------------------------
NGWWFSServiceLayer::NGWWFSServiceLayer(const std::string &key,
                                       const std::string &name,
                                       NGWResourceBase *resource,
                                       int maxfeatures) :
    NGWServiceLayer(key, name, resource),
    m_maxfeatures(maxfeatures)
{

}

NGWWFSServiceLayer::NGWWFSServiceLayer(const std::string &key,
                                       const std::string &name,
                                       int resourceId, int maxfeatures) :
    NGWServiceLayer(key, name, resourceId),
    m_maxfeatures(maxfeatures)
{

}

//------------------------------------------------------------------------------
// NGWWMSServiceLayer
//------------------------------------------------------------------------------
NGWWMSServiceLayer::NGWWMSServiceLayer(const std::string &key,
                                       const std::string &name,
                                       NGWResourceBase *resource,
                                       const std::string &minScaleDenom,
                                       const std::string &maxScaleDenom) :
    NGWServiceLayer(key, name, resource),
    m_minScaleDenom(minScaleDenom),
    m_maxScaleDenom(maxScaleDenom)
{

}

NGWWMSServiceLayer::NGWWMSServiceLayer(const std::string &key,
                                       const std::string &name,
                                       int resourceId,
                                       const std::string &minScaleDenom,
                                       const std::string &maxScaleDenom) :
    NGWServiceLayer(key, name, resourceId),
    m_minScaleDenom(minScaleDenom),
    m_maxScaleDenom(maxScaleDenom)
{

}

//------------------------------------------------------------------------------
// NGWStyle
//------------------------------------------------------------------------------
NGWStyle::NGWStyle(ObjectContainer * const parent,
                   const enum ngsCatalogObjectType type,
                   const std::string &name,
                   const CPLJSONObject &resource,
                   NGWConnectionBase *connection) :
    Raster(std::vector<std::string>(), parent, type, name, ""),
    NGWResourceBase(resource, connection)
{
    if(type == CAT_NGW_MAPSERVER_STYLE && resource.IsValid()) {
        m_style = resource.GetString("mapserver_style/xml");
    }

    m_path = "NGW:" + url() + "/resource/" + m_resourceId;

    // TODO: Add addNotifyReceiver to get if layer was deleted
    // Add deleteNotifyReceiver on destruct
}

NGWStyle *NGWStyle::createStyle(NGWResourceBase *parent,
                                const enum ngsCatalogObjectType type,
                                const std::string &name, const Options &options)
{
    resetError();
    auto connection = parent->connection();
    auto url = connection->connectionUrl();

    auto stylePath = options.asString("STYLE_PATH");
    auto styleStr = options.asString("STYLE_STRING");

    CPLJSONObject payload = createResourcePayload(parent, type, name, options);

    CPLJSONObject tileCache("tile_cache", payload);
    tileCache.Add("enabled", options.asBool("CACHE_ENABLED", false));
    tileCache.Add("image_compose", options.asBool("CACHE_IMAGE_COMPOSE", false));
    tileCache.Add("max_z", options.asString("CACHE_MAX_Z", "5"));
    tileCache.Add("ttl", options.asString("CACHE_TTL", "2630000"));
    tileCache.Add("track_changes", options.asBool("CACHE_TRACK_CHANGES", false));

    if(type == CAT_NGW_QGISVECTOR_STYLE || type == CAT_NGW_QGISRASTER_STYLE) {
        std::string deletePath;
        if(stylePath.empty()) {
            if(styleStr.empty()) {
                errorMessage(_("STYLE_PATH or STYLE_STRING options must be present"));
                return nullptr;
            }

            Settings &settings = Settings::instance();
            auto tmpDir = settings.getString("common/cache_path", "");
            if(tmpDir.empty()) {
                errorMessage(_("Cache path option must be present"));
                return nullptr;
            }

            stylePath = File::formFileName(tmpDir, random(10));
            if(!File::writeFile(stylePath, styleStr.data(), styleStr.size())) {
                return nullptr;
            }
            deletePath = stylePath;
        }
        // Upload file
        auto uploadInfo = http::uploadFile(ngw::getUploadUrl(url), stylePath);
        if(!deletePath.empty()) {
            File::deleteFile(deletePath);
        }
        // {"upload_meta": [{"id": "9226e604-cdbe-4719-842b-d180970100c7", "name": "96.qml", "mime_type": "application/octet-stream", "size": 1401}]}
        auto uploadMetaArray = uploadInfo.GetArray("upload_meta");
        if(uploadMetaArray.Size() == 0) {
            errorMessage(_("Failed upload style"));
            return nullptr;
        }
        auto uploadMeta = uploadMetaArray[0];
        auto size = uploadMeta.GetLong("size");
        auto id = uploadMeta.GetString("id");
        auto mime = uploadMeta.GetString("mime_type");
        auto uploadName = uploadMeta.GetString("name");

        CPLJSONObject style(ngw::objectTypeToNGWClsType(type), payload);
        CPLJSONObject upload("file_upload", style);
        upload.Add("id", id);
        upload.Add("name", uploadName);
        upload.Add("mime_type", mime);
        upload.Add("size", size);
//        "qgis_vector_style": {
//                "file_upload": {
//                    "id": "958314eb-d0f3-4fec-ab1b-714bda784141",
//                    "name": "39.qml",
//                    "mime_type": "application/octet-stream",
//                    "size": 12349
//                }
//            }
    }
    else if(type == CAT_NGW_MAPSERVER_STYLE) {
        if(styleStr.empty()) {
            if(stylePath.empty()) {
                errorMessage(_("STYLE_PATH or STYLE_STRING options must be present"));
                return nullptr;
            }
            styleStr = File::readFile(stylePath);
        }

        CPLJSONObject style("mapserver_style", payload);
        style.Add("xml", styleStr);
//            "mapserver_style": {
//                "xml": "<map>\n  <layer>\n    <class>\n      <style>\n        <color blue=\"179\" green=\"255\" red=\"255\"/>\n        <outlinecolor blue=\"64\" green=\"64\" red=\"64\"/>\n      </style>\n    </class>\n  </layer>\n  <legend>\n    <keysize y=\"15\" x=\"15\"/>\n    <label>\n      <size>12</size>\n      <type>truetype</type>\n      <font>regular</font>\n    </label>\n  </legend>\n</map>\n"
//            }
    }
    else if(type == CAT_NGW_RASTER_STYLE) {

    }
    else {
        errorMessage(_("Unsupported type %d"), type);
        return nullptr;
    }

    std::string resourceId = ngw::createResource(url,
        payload.Format(CPLJSONObject::PrettyFormat::Plain), http::getGDALHeaders(url).StealList());
    if(compare(resourceId, "-1", true)) {
        return nullptr;
    }

    payload.Add("resource/id", atoi(resourceId.c_str()));
    return new NGWStyle(dynamic_cast<ObjectContainer*>(parent), type, name,
                        payload, connection);
}

bool NGWStyle::destroy()
{
    if(!NGWResourceBase::remove()) {
        return false;
    }

    return Object::destroy();
}

bool NGWStyle::canDestroy() const
{
    return true;
}

Properties NGWStyle::properties(const std::string &domain) const
{
    auto out = Raster::properties(domain);
    out.append(NGWResourceBase::metadata(domain));
    if(domain.empty()) {
        out.add("style", m_style);
        out.add("style_path", m_stylePath);
    }
    return out;
}

std::string NGWStyle::property(const std::string &key,
                               const std::string &defaultValue,
                               const std::string &domain) const
{
    if(!domain.empty()) {
        return Raster::property(key, defaultValue, domain);
    }

    if(compare(key, "style")) {
        return m_style;
    }

    if(compare(key, "style_path")) {
        return m_stylePath;
    }

    return NGWResourceBase::metadataItem(key, defaultValue, domain);
}

bool NGWStyle::setProperty(const std::string &key, const std::string &value,
                           const std::string &domain)
{
    if(!domain.empty()) {
        return Raster::setProperty(key, value, domain);
    }

    if(compare(key, "style")) {
        if(m_type == CAT_NGW_MAPSERVER_STYLE) {
            m_style = value;
        }
        else {
            m_stylePath = value;
        }
        return sync();
    }

    if(compare(key, "style_path")) {
        if(m_type == CAT_NGW_MAPSERVER_STYLE) {
            m_style = File::readFile(value);
        }
        else {
            m_stylePath = value;
        }
        return sync();
    }

    if(m_DS != nullptr) {
        return m_DS->SetMetadataItem(key.c_str(), value.c_str(),
                                       domain.c_str()) == CE_None;
    }

    return false;
}

CPLJSONObject NGWStyle::asJson() const
{
    auto payload = NGWResourceBase::asJson();

    CPLJSONObject resource = payload.GetObj("resource");
    resource.Add("display_name", m_name);

    if(m_type == CAT_NGW_MAPSERVER_STYLE) {
        CPLJSONObject style("mapserver_style", payload);
        style.Add("xml", m_style);
    }
    else if(m_type == CAT_NGW_QGISRASTER_STYLE ||
            m_type == CAT_NGW_QGISVECTOR_STYLE) {
        auto uploadInfo = http::uploadFile(ngw::getUploadUrl(url()), m_stylePath);
        auto uploadMetaArray = uploadInfo.GetArray("upload_meta");
        if(uploadMetaArray.Size() == 0) {
            return payload;
        }
        auto uploadMeta = uploadMetaArray[0];
        auto size = uploadMeta.GetLong("size");
        auto id = uploadMeta.GetString("id");
        auto mime = uploadMeta.GetString("mime_type");
        auto uploadName = uploadMeta.GetString("name");

        CPLJSONObject style("qgis_vector_style", payload);
        CPLJSONObject upload("file_upload", style);
        upload.Add("id", id);
        upload.Add("name", uploadName);
        upload.Add("mime_type", mime);
        upload.Add("size", size);
    }
    return payload;
}

//------------------------------------------------------------------------------
// NGWWebmap
//------------------------------------------------------------------------------
NGWWebMap::NGWWebMap(ObjectContainer * const parent, const std::string &name,
                     const CPLJSONObject &resource,
                     NGWConnectionBase *connection) :
    NGWResource(parent, CAT_NGW_WEBMAP, name, resource, connection),
    m_drawOrderEnabled(false),
    m_editable(false),
    m_annotationEnabled(false),
    m_annotationDefault(false),
    m_bookmarkResourceId(NOT_FOUND),
    m_layerTree(new NGWWebMapRoot(connection))
{
    if(resource.IsValid()) {
        auto left = resource.GetDouble("webmap/extent_left", 0.0);
        auto right = resource.GetDouble("webmap/extent_right", 0.0);
        auto bottom = resource.GetDouble("webmap/extent_bottom", 0.0);
        auto top = resource.GetDouble("webmap/extent_top", 0.0);
        m_extent = Envelope(left, bottom, right, top);

        m_drawOrderEnabled = resource.GetBool("webmap/draw_order_enabled", false);
        m_editable = resource.GetBool("webmap/editable", false);
        m_annotationEnabled = resource.GetBool("webmap/annotation_enabled", false);
        m_annotationDefault = resource.GetBool("webmap/annotation_default", false);
        m_bookmarkResourceId = resource.GetLong("webmap/bookmark_resource/id", -1);

        fill(resource.GetObj("webmap/root_item"));
        fillBasemaps(resource.GetArray("basemap_webmap/basemaps"));
    }

    // TODO: Add addNotifyReceiver to get if basemap or webmap layer was deleted
    // Add deleteNotifyReceiver on destruct
}


Properties NGWWebMap::properties(const std::string &domain) const
{
    auto out = NGWResource::properties(domain);
    if(domain.empty()) {
        out.add("draw_order_enabled", fromBool(m_drawOrderEnabled));
        out.add("editable", fromBool(m_editable));
        out.add("annotation_enabled", fromBool(m_annotationEnabled));
        out.add("annotation_default", fromBool(m_annotationDefault));
    }
    return out;
}

std::string NGWWebMap::property(const std::string &key,
                               const std::string &defaultValue,
                               const std::string &domain) const
{
    if(!domain.empty()) {
        return NGWResource::property(key, defaultValue, domain);
    }

    if(compare(key, "draw_order_enabled")) {
        return fromBool(m_drawOrderEnabled);
    }

    if(compare(key, "editable")) {
        return fromBool(m_editable);
    }

    if(compare(key, "annotation_enabled")) {
        return fromBool(m_annotationEnabled);
    }

    if(compare(key, "annotation_default")) {
        return fromBool(m_annotationDefault);
    }

    return NGWResource::property(key, defaultValue, domain);
}

bool NGWWebMap::setProperty(const std::string &key, const std::string &value,
                           const std::string &domain)
{
    if(NGWResource::setProperty(key, value, domain)) {
        return true;
    }

    if(compare(key, "draw_order_enabled")) {
        m_drawOrderEnabled = toBool(value);
        return true;
    }

    if(compare(key, "editable")) {
        m_editable = toBool(value);
        return true;
    }

    if(compare(key, "annotation_enabled")) {
        m_annotationEnabled = toBool(value);
        return true;
    }

    if(compare(key, "annotation_default")) {
        m_annotationDefault = toBool(value);
        return true;
    }

    return false;
}


/*
"webmap": {
  "extent_left": 35.521,
  "extent_right": 35.735,
  "extent_bottom": 35.059,
  "extent_top": 35.279,
  "draw_order_enabled": false,
  "editable": false,
  "annotation_enabled": false,
  "annotation_default": false,
  "bookmark_resource": null,
  "root_item": {
    "item_type": "root",
    "children": [
      {
        "group_expanded": true,
        "display_name": "МАТЕМАТИЧЕСКАЯ ОСНОВА",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": false,
            "style_parent_id": 163,
            "draw_order_position": 2,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "МАТЕМАТИЧЕСКАЯ ОСНОВА_lin_layer1",
            "layer_style_id": 164,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": true,
        "display_name": "НАЗВАНИЯ И ПОДПИСИ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": false,
            "style_parent_id": 166,
            "draw_order_position": 3,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "НАЗВАНИЯ И ПОДПИСИ_dot_layer17",
            "layer_style_id": 167,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": false,
            "style_parent_id": 168,
            "draw_order_position": 4,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "НАЗВАНИЯ И ПОДПИСИ_lin_layer17",
            "layer_style_id": 169,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": true,
        "display_name": "ГРАНИЦЫ И ОГРАЖДЕНИЯ",
        "children": [
          {
            "layer_adapter": "tile",
            "layer_enabled": true,
            "style_parent_id": 171,
            "draw_order_position": 5,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ГРАНИЦЫ И ОГРАЖДЕНИЯ_lin_layer14",
            "layer_style_id": 172,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": false,
        "display_name": "РЕЛЬЕФ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 174,
            "draw_order_position": 6,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "РЕЛЬЕФ СУШИ_dot_layer5",
            "layer_style_id": 175,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 176,
            "draw_order_position": 7,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ГИДРОГРАФИЯ (РЕЛЬЕФ)_dot_layer8",
            "layer_style_id": 177,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 178,
            "draw_order_position": 8,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ГИДРОГРАФИЯ (РЕЛЬЕФ)_lin_layer8",
            "layer_style_id": 179,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 180,
            "draw_order_position": 9,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "РЕЛЬЕФ СУШИ_lin_layer5",
            "layer_style_id": 181,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 182,
            "draw_order_position": 10,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "РЕЛЬЕФ СУШИ_sqr_layer5",
            "layer_style_id": 183,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": false,
        "display_name": "ДОРОЖНАЯ СЕТЬ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 185,
            "draw_order_position": 11,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ДОРОЖНАЯ СЕТЬ_lin_layer10",
            "layer_style_id": 186,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 187,
            "draw_order_position": 12,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ДОРОЖНЫЕ СООРУЖЕНИЯ_lin_layer11",
            "layer_style_id": 188,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 189,
            "draw_order_position": 13,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "НАСЫПИ,ВЫЕМКИ,ЭСТАКАДЫ_lin_layer16",
            "layer_style_id": 190,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": false,
        "display_name": "ЗАПОЛНЯЮЩИЕ ЗНАКИ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": false,
            "style_parent_id": 192,
            "draw_order_position": 14,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ЗАПОЛНЯЮЩИЕ ЗНАКИ_dot_layer21",
            "layer_style_id": 193,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": false,
            "style_parent_id": 194,
            "draw_order_position": 15,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ЗАПОЛНЯЮЩИЕ ЗНАКИ_lin_layer21",
            "layer_style_id": 195,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": false,
            "style_parent_id": 196,
            "draw_order_position": 16,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ЗАПОЛНЯЮЩИЕ ЗНАКИ_sqr_layer21",
            "layer_style_id": 197,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "layer_adapter": "image",
        "layer_enabled": true,
        "style_parent_id": 198,
        "draw_order_position": 17,
        "layer_max_scale_denom": null,
        "item_type": "layer",
        "layer_min_scale_denom": null,
        "display_name": "КАМЫШОВЫЕ,МАНГРОВЫЕ ЗАРОСЛИ_sqr_layer18",
        "layer_style_id": 199,
        "layer_transparency": null
      },
      {
        "group_expanded": false,
        "display_name": "ГИДРОГРАФИЯ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 201,
            "draw_order_position": 18,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ГИДРОТЕХНИЧЕСКИЕ СООРУЖЕНИЯ_dot_layer9",
            "layer_style_id": 202,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 203,
            "draw_order_position": 19,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ГИДРОТЕХНИЧЕСКИЕ СООРУЖЕНИЯ_lin_layer9",
            "layer_style_id": 204,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 205,
            "draw_order_position": 20,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ГИДРОГРАФИЯ_lin_layer7",
            "layer_style_id": 206,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 207,
            "draw_order_position": 21,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ГИДРОГРАФИЯ_sqr_layer7",
            "layer_style_id": 208,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": false,
        "display_name": "ПРОМЫШЛЕН.И СОЦИАЛЬНЫЕ ОБ'ЕКТ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 210,
            "draw_order_position": 22,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ПРОМЫШЛЕН.И СОЦИАЛЬНЫЕ ОБЕКТ_dot_layer13",
            "layer_style_id": 211,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 212,
            "draw_order_position": 23,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ПРОМЫШЛЕН.И СОЦИАЛЬНЫЕ ОБЕКТ_lin_layer13",
            "layer_style_id": 213,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 214,
            "draw_order_position": 1,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ПРОМЫШЛЕН.И СОЦИАЛЬНЫЕ ОБЕКТ_sqr_layer13",
            "layer_style_id": 215,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": false,
        "display_name": "НАСЕЛЕННЫЕ ПУНКТЫ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 217,
            "draw_order_position": 24,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "НАСЕЛЕННЫЕ ПУНКТЫ (СТРОЕНИЯ)_lin_layer20",
            "layer_style_id": 218,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 219,
            "draw_order_position": 25,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "НАСЕЛЕННЫЕ ПУНКТЫ (КВАРТАЛЫ)_lin_layer12",
            "layer_style_id": 220,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 221,
            "draw_order_position": 26,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "НАСЕЛЕННЫЕ ПУНКТЫ (КВАРТАЛЫ)_sqr_layer12",
            "layer_style_id": 222,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 223,
            "draw_order_position": 27,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "НАСЕЛЕННЫЕ ПУНКТЫ (СТРОЕНИЯ)_sqr_layer20",
            "layer_style_id": 224,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 225,
            "draw_order_position": 28,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": 5000.0,
            "display_name": "КВАРТАЛЫ (НЕОДНОРОДНЫЕ)_sqr_layer19",
            "layer_style_id": 226,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 227,
            "draw_order_position": 29,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "НАСЕЛЕННЫЕ ПУНКТЫ_sqr_layer2",
            "layer_style_id": 228,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": false,
        "display_name": "ГРУНТЫ И ЛАВОВЫЕ ПОКРОВЫ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 230,
            "draw_order_position": 30,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ГРУНТЫ И ЛАВОВЫЕ ПОКРОВЫ_sqr_layer4",
            "layer_style_id": 231,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": false,
        "display_name": "РАСТИТЕЛЬНОСТЬ",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 233,
            "draw_order_position": 31,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "РАСТИТЕЛЬНОСТЬ_dot_layer6",
            "layer_style_id": 234,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 235,
            "draw_order_position": 32,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "РАСТИТЕЛЬНОСТЬ_lin_layer6",
            "layer_style_id": 236,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 237,
            "draw_order_position": 33,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "РАСТИТЕЛЬНОСТЬ_sqr_layer6",
            "layer_style_id": 238,
            "layer_transparency": null
          },
          {
            "layer_adapter": "image",
            "layer_enabled": true,
            "style_parent_id": 239,
            "draw_order_position": 34,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "РАСТИТЕЛЬНОСТЬ (ЗАЛИВКА),ТАКЫР_sqr_layer3",
            "layer_style_id": 240,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "layer_adapter": "image",
        "layer_enabled": true,
        "style_parent_id": 241,
        "draw_order_position": 35,
        "layer_max_scale_denom": null,
        "item_type": "layer",
        "layer_min_scale_denom": null,
        "display_name": "ГИДРОГРАФИЯ (РЕЛЬЕФ)_sqr_layer8",
        "layer_style_id": 242,
        "layer_transparency": null
      },
      {
        "group_expanded": false,
        "display_name": "ПЛАНОВО-ВЫСОТНАЯ ОСНОВА",
        "children": [
          {
            "layer_adapter": "image",
            "layer_enabled": false,
            "style_parent_id": 244,
            "draw_order_position": 36,
            "layer_max_scale_denom": null,
            "item_type": "layer",
            "layer_min_scale_denom": null,
            "display_name": "ПЛАНОВО-ВЫСОТНАЯ ОСНОВА_dot_layer15",
            "layer_style_id": 245,
            "layer_transparency": null
          }
        ],
        "item_type": "group"
      },
      {
        "group_expanded": false,
        "display_name": "ОБЪЕКТЫ ДЛЯ КАРТОИЗДАНИЯ",
        "children": [],
        "item_type": "group"
      }
    ]
  }
},
"basemap_webmap": {
"basemaps": [
      {
        "opacity": null,
        "enabled": true,
        "position": 0,
        "display_name": "Спутник",
        "resource_id": 3914
      },
      {
        "opacity": null,
        "enabled": true,
        "position": 1,
        "display_name": "OpenStreetMap.de",
        "resource_id": 3913
      }
    ]
},
*/

NGWWebMap *NGWWebMap::create(NGWResourceBase *parent, const std::string &name,
                             const Options &options)
{
    resetError();
    auto connection = parent->connection();
    auto url = connection->connectionUrl();

    CPLJSONObject payload = createResourcePayload(parent, CAT_NGW_WEBMAP, name, options);

    CPLJSONObject webmap("webmap", payload);
    webmap.Add("extent_left", -180.0);
    webmap.Add("extent_right", 180.0);
    webmap.Add("extent_bottom", -90.0);
    webmap.Add("extent_top", 90.0);
    webmap.Add("draw_order_enabled", false);
    webmap.Add("editable", false);
    webmap.Add("annotation_enabled", false);
    webmap.Add("annotation_default", false);

    CPLJSONObject rootItem("root_item", webmap);
    rootItem.Add("item_type", "root");
    rootItem.Add("children", CPLJSONArray());

    std::string resourceId = ngw::createResource(url,
        payload.Format(CPLJSONObject::PrettyFormat::Plain), http::getGDALHeaders(url).StealList());
    if(compare(resourceId, "-1", true)) {
        return nullptr;
    }

    payload.Add("resource/id", atoi(resourceId.c_str()));
    return new NGWWebMap(dynamic_cast<ObjectContainer*>(parent), name,
                         payload, connection);
}

void NGWWebMap::fillBasemaps(const CPLJSONArray &basemaps)
{
    if(!basemaps.IsValid()) {
        return;
    }

    auto ngwConn = dynamic_cast<NGWConnection*>(m_connection);
    if(ngwConn) {
        return;
    }
    std::map<int, BaseMap> bm;
    for(int i = 0; i < basemaps.Size(); ++i) {
        auto basemap = basemaps[i];
        BaseMap item;
        item.opacity = basemap.GetInteger("opacity");
        item.enabled = basemap.GetBool("enabled");
        int pos = basemap.GetInteger("position");
        item.displayName = basemap.GetString("display_name");

        auto baseMapResource = ngwConn->getResource(basemap.GetString("resource_id"));

        item.resource = baseMapResource;
        bm[pos] = item;
    }
    m_baseMaps.clear();
    for(const auto &item : bm) {
        m_baseMaps.emplace_back(item.second);
    }
}

void NGWWebMap::fill(const CPLJSONObject &layers)
{
    m_layerTree->clear();
    m_layerTree->fill(layers);
}

NGWWebMapRootPtr NGWWebMap::layerTree() const
{
    return m_layerTree;
}

bool NGWWebMap::deleteItem(intptr_t id)
{
    return m_layerTree->deleteItem(id);
}

intptr_t NGWWebMap::insetItem(intptr_t pos, NGWWebMapItem *item)
{
    return m_layerTree->insetItem(pos, item);
}

static bool isBaseMapValid(const NGWWebMap::BaseMap &basemap)
{
    return basemap.resource && basemap.resource->type() == CAT_NGW_BASEMAP;
}

std::vector<NGWWebMap::BaseMap> NGWWebMap::baseMaps() const
{
    return m_baseMaps;
}

bool NGWWebMap::addBaseMap(const NGWWebMap::BaseMap &basemap)
{
    if(!isBaseMapValid(basemap)) {
        return false;
    }
    m_baseMaps.emplace_back(basemap);
    return true;
}

bool NGWWebMap::insertBaseMap(int index, const NGWWebMap::BaseMap &basemap)
{
    if(!isBaseMapValid(basemap)) {
        return false;
    }
    m_baseMaps.insert(m_baseMaps.begin() + index, basemap);
    return true;
}

bool NGWWebMap::deleteBaseMap(int index)
{
    m_baseMaps.erase(m_baseMaps.begin() + index);
    return true;
}

CPLJSONObject NGWWebMap::asJson() const
{
    auto payload = NGWResourceBase::asJson();

    auto resource = payload.GetObj("resource");
    resource.Add("display_name", m_name);

    CPLJSONObject webmap("webmap", payload);
    webmap.Add("extent_left", m_extent.minX());
    webmap.Add("extent_right", m_extent.maxX());
    webmap.Add("extent_bottom", m_extent.minY());
    webmap.Add("extent_top", m_extent.maxY());
    webmap.Add("draw_order_enabled", m_drawOrderEnabled);
    webmap.Add("editable", m_editable);
    webmap.Add("annotation_enabled", m_annotationEnabled);
    webmap.Add("annotation_default", m_annotationDefault);

    if(m_bookmarkResourceId == NOT_FOUND) {
        webmap.SetNull("bookmark_resource");
    }
    else {
        CPLJSONObject bookmarkResource("bookmark_resource", webmap);
        bookmarkResource.Add("id", static_cast<GInt64>(m_bookmarkResourceId));
    }

    webmap.Add("root_item", m_layerTree->asJson());

    CPLJSONArray baseMaps;
    int pos = 0;
    for(const auto &item : m_baseMaps) {
        CPLJSONObject baseMap;
        baseMap.Add("opacity", item.opacity);
        baseMap.Add("enabled", item.enabled);
        baseMap.Add("position", pos++);
        baseMap.Add("display_name", item.displayName);

        auto ngwResource = ngsDynamicCast(NGWResourceBase, item.resource);
        if(ngwResource) {
            baseMap.Add("resource_id", atoi(ngwResource->resourceId().c_str()));
        }
        baseMaps.Add(baseMap);
    }

    CPLJSONObject basemapWebmap("basemap_webmap", payload);
    basemapWebmap.Add("basemaps", baseMaps);

    return payload;
}

//------------------------------------------------------------------------------
// NGWBaseMap
//------------------------------------------------------------------------------
static std::string getUrl(const std::string &qms)
{
    CPLJSONDocument doc;
    if(doc.LoadMemory(qms)) {
        auto root = doc.GetRoot();
        if(root.IsValid()) {
            return  root.GetString("url");
        }
    }
    return "";
}

NGWBaseMap::NGWBaseMap(ObjectContainer * const parent, const std::string &name,
                       const CPLJSONObject &resource,
                       NGWConnectionBase *connection) :
       Raster(std::vector<std::string>(), parent, CAT_NGW_BASEMAP, name, ""),
       NGWResourceBase(resource, connection)
{
    if(resource.IsValid()) {
        m_url = resource.GetString("basemap_layer/url");
        m_qms = resource.GetString("basemap_layer/qms");
    }
}

NGWBaseMap *NGWBaseMap::create(NGWResourceBase *parent,
                                      const std::string &name,
                                      const Options &options)
{
    resetError();
    auto connection = parent->connection();
    auto url = connection->connectionUrl();

    auto bmUrl = options.asString(URL_KEY);
    std::string qmsStr;
    if(bmUrl.empty()) {
        auto bmQmsId = options.asInt("QMS_ID");
        auto qmsJson = qms::QMSItemProperties(bmQmsId);
        qmsStr = qmsJson.Format(CPLJSONObject::PrettyFormat::Plain);
        bmUrl = getUrl(qmsStr);
    }

    CPLJSONObject payload = createResourcePayload(parent, CAT_NGW_BASEMAP, name, options);

    CPLJSONObject basemap("basemap_layer", payload);
    basemap.Add("url", bmUrl);
    if(!qmsStr.empty()) {
        basemap.Add("qms", qmsStr);
    }

    std::string resourceId = ngw::createResource(url,
        payload.Format(CPLJSONObject::PrettyFormat::Plain), http::getGDALHeaders(url).StealList());
    if(compare(resourceId, "-1", true)) {
        return nullptr;
    }

    payload.Add("resource/id", atoi(resourceId.c_str()));
    return new NGWBaseMap(dynamic_cast<ObjectContainer*>(parent), name,
                          payload, connection);
}

bool NGWBaseMap::destroy()
{
    if(!NGWResourceBase::remove()) {
        return false;
    }

    return Object::destroy();
}

bool NGWBaseMap::canDestroy() const
{
    return true;
}

Properties NGWBaseMap::properties(const std::string &domain) const
{
    auto out = Raster::properties(domain);    
    out.append(NGWResourceBase::metadata(domain));
    if(domain.empty()) {
        out.add("url", m_url);
        out.add("qms", m_qms);
    }
    return out;
}

std::string NGWBaseMap::property(const std::string &key,
                               const std::string &defaultValue,
                               const std::string &domain) const
{
    if(!domain.empty()) {
        return Raster::property(key, defaultValue, domain);
    }

    if(compare(key, "url")) {
        return m_url;
    }

    if(compare(key, "qms")) {
        return m_qms;
    }

    return NGWResourceBase::metadataItem(key, defaultValue, domain);
}

bool NGWBaseMap::setProperty(const std::string &key, const std::string &value,
                           const std::string &domain)
{
    if(!domain.empty()) {
        return Raster::setProperty(key, value, domain);
    }

    if(compare(key, "url")) {
        m_url = value;
        m_qms.clear();
        return sync();
    }

    if(compare(key, "qms")) {
        m_qms = value;
        m_url = getUrl(m_qms);
        return sync();
    }

    if(compare(key, "qms_id")) {
        auto qmsJson = qms::QMSItemProperties(atoi(value.c_str()));
        m_qms = qmsJson.Format(CPLJSONObject::PrettyFormat::Plain);
        m_url = getUrl(m_qms);
        return sync();
    }

    if(m_DS != nullptr) {
        return m_DS->SetMetadataItem(key.c_str(), value.c_str(),
                                       domain.c_str()) == CE_None;
    }

    return false;
}

CPLJSONObject NGWBaseMap::asJson() const
{
    auto payload = NGWResourceBase::asJson();
    auto resource = payload.GetObj("resource");
    resource.Add("display_name", m_name);

    CPLJSONObject basemap("basemap_layer", payload);
    basemap.Add("url", m_url);
    if(m_qms.empty()) {
        basemap.SetNull("qms");
    }
    else {
        basemap.Add("qms", m_qms);
    }
    return payload;
}

bool NGWBaseMap::open(unsigned int openFlags, const Options &options)
{
    if(isOpened()) {
        return true;
    }

    m_openFlags = openFlags;
    m_openOptions = options;

    CPLString url(m_url);
    url = url.replaceAll("{", "${");
    url = url.replaceAll("&", "&amp;");
    int epsg = DEFAULT_EPSG;
    int z_min = 0;
    int z_max = DEFAULT_MAX_ZOOM;
    bool y_origin_top = true;
    unsigned short bandCount = DEFAULT_BAND_COUNT;

    if(!m_qms.empty()) {
        CPLJSONDocument doc;
        if(doc.LoadMemory(m_qms)) {
            auto root = doc.GetRoot();
            if(root.IsValid()) {
                epsg = root.GetInteger("epsg", epsg);
                z_min = root.GetInteger("z_min", z_min);
                z_max = root.GetInteger("z_max", z_max);
                y_origin_top = root.GetInteger("y_origin_top", y_origin_top);
            }
        }
    }

    m_spatialReference = SpatialReferencePtr::importFromEPSG(epsg);

    Envelope extent = DEFAULT_BOUNDS;
    m_extent =  DEFAULT_BOUNDS;
    int cacheExpires = defaultCacheExpires;
    int cacheMaxSize = defaultCacheMaxSize;

    const Settings &settings = Settings::instance();
    int timeout = settings.getInteger("http/timeout", 5);

    const char *connStr = CPLSPrintf("<GDAL_WMS><Service name=\"TMS\">"
        "<ServerUrl>%s</ServerUrl></Service><DataWindow>"
        "<UpperLeftX>%f</UpperLeftX><UpperLeftY>%f</UpperLeftY>"
        "<LowerRightX>%f</LowerRightX><LowerRightY>%f</LowerRightY>"
        "<TileLevel>%d</TileLevel><TileCountX>1</TileCountX>"
        "<TileCountY>1</TileCountY><YOrigin>%s</YOrigin></DataWindow>"
        "<Projection>EPSG:%d</Projection><BlockSizeX>256</BlockSizeX>"
        "<BlockSizeY>256</BlockSizeY><BandsCount>%d</BandsCount>"
        "<Cache><Type>file</Type><Expires>%d</Expires><MaxSize>%d</MaxSize>"
        "</Cache><MaxConnections>1</MaxConnections><Timeout>%d</Timeout><AdviseRead>false</AdviseRead>"
        "<ZeroBlockHttpCodes>204,404</ZeroBlockHttpCodes></GDAL_WMS>",
        url.c_str(), extent.minX(), extent.maxY(), extent.maxX(), extent.minY(), z_max,
        y_origin_top ? "top" : "bottom", epsg, bandCount, cacheExpires, cacheMaxSize, timeout);

    bool result = DatasetBase::open(connStr, openFlags, options);
    if(result) {
        // Set NG_ADDITIONS metadata
        m_DS->SetMetadataItem("TMS_URL", url.c_str(), "");
        m_DS->SetMetadataItem("TMS_CACHE_EXPIRES", CPLSPrintf("%d", cacheExpires), "");
        m_DS->SetMetadataItem("TMS_CACHE_MAX_SIZE", CPLSPrintf("%d", cacheMaxSize), "");
        m_DS->SetMetadataItem("TMS_Y_ORIGIN_TOP", y_origin_top ? "top" : "bottom", "");
        m_DS->SetMetadataItem("TMS_Z_MIN", CPLSPrintf("%d", z_min), "");
        m_DS->SetMetadataItem("TMS_Z_MAX", CPLSPrintf("%d", z_max), "");
        m_DS->SetMetadataItem("TMS_X_MIN", CPLSPrintf("%f", extent.minX()), "");
        m_DS->SetMetadataItem("TMS_X_MAX", CPLSPrintf("%f", extent.maxX()), "");
        m_DS->SetMetadataItem("TMS_Y_MIN", CPLSPrintf("%f", extent.minY()), "");
        m_DS->SetMetadataItem("TMS_Y_MAX", CPLSPrintf("%f", extent.maxY()), "");

        m_DS->SetMetadataItem("TMS_LIMIT_X_MIN", CPLSPrintf("%f", m_extent.minX()), "");
        m_DS->SetMetadataItem("TMS_LIMIT_X_MAX", CPLSPrintf("%f", m_extent.maxX()), "");
        m_DS->SetMetadataItem("TMS_LIMIT_Y_MIN", CPLSPrintf("%f", m_extent.minY()), "");
        m_DS->SetMetadataItem("TMS_LIMIT_Y_MAX", CPLSPrintf("%f", m_extent.maxY()), "");

        // Set USER metadata
        for(const auto &resmetaItem : m_resmeta) {
            m_DS->SetMetadataItem(resmetaItem.first.c_str(),
                                  resmetaItem.second.c_str(), USER_KEY);
        }

        // Set pixel limits from m_extent
        double geoTransform[6] = { 0.0 };
        double invGeoTransform[6] = { 0.0 };
        bool noTransform = false;
        if(m_DS->GetGeoTransform(geoTransform) == CE_None) {
            noTransform = GDALInvGeoTransform(geoTransform, invGeoTransform) == 0;
        }

        if(noTransform) {
            double minX, minY, maxX, maxY;
            GDALApplyGeoTransform(invGeoTransform, m_extent.minX(),
                                  m_extent.minY(), &minX, &maxY);

            GDALApplyGeoTransform(invGeoTransform, m_extent.maxX(),
                                  m_extent.maxY(), &maxX, &minY);
            m_pixelExtent = Envelope(minX, minY, maxX, maxY);
        }
        else {
            m_pixelExtent = Envelope(0.0, 0.0,
                std::numeric_limits<double>::max(),
                std::numeric_limits<double>::max());
        }
    }
    return result;
}

//------------------------------------------------------------------------------
// NGWWebMapItem
//------------------------------------------------------------------------------

NGWWebMapItem::NGWWebMapItem(NGWConnectionBase *connection) :
    m_id(reinterpret_cast<intptr_t>(this)),
    m_connection(connection)
{

}

//------------------------------------------------------------------------------
// NGWWebMapGroup
//------------------------------------------------------------------------------
NGWWebMapGroup::NGWWebMapGroup(NGWConnectionBase *connection) :
    NGWWebMapItem(connection)
{
    m_type = NGWWebMapItem::ItemType::GROUP;
}

void NGWWebMapGroup::clear()
{
    m_children.clear();
}

CPLJSONObject NGWWebMapGroup::asJson() const
{
    CPLJSONObject out;
    if(m_type == NGWWebMapItem::ItemType::ROOT) {
        out.Add("item_type", "root");
    }
    else if(m_type == NGWWebMapItem::ItemType::GROUP) {
        out.Add("item_type", "group");
        out.Add("display_name", m_displayName);
        out.Add("group_expanded", m_expanded);
    }
    else {
        return out;
    }

    CPLJSONArray children;
    for(const auto &child : m_children) {
        children.Add(child->asJson());
    }

    out.Add("children", children);
    return out;
}

NGWWebMapItem *NGWWebMapGroup::clone()
{
    auto out = new NGWWebMapGroup(m_connection);
    out->m_expanded = m_expanded;
    out->m_id = m_id;
    out->m_type = m_type;
    out->m_displayName = m_displayName;
    for(const auto &child : m_children) {
        out->m_children.emplace_back(child->clone());
    }
    return out;
}

bool NGWWebMapGroup::fill(const CPLJSONObject &item)
{
    auto itemType = item.GetString("item_type");
    if(compare(itemType, "root")) {
        m_type = NGWWebMapItem::ItemType::ROOT;
    }
    else if(compare(itemType, "group")) {
        m_type = NGWWebMapItem::ItemType::GROUP;
        m_expanded = item.GetBool("group_expanded", false);
        m_displayName = item.GetString("display_name");
    }
    else {
        return false;
    }

    auto children = item.GetArray("children");
    if(children.IsValid()) {
        for(int i = 0; i < children.Size(); ++i) {
            auto child = children[i];
            auto childType = child.GetString("item_type");
            NGWWebMapItem *newItem = nullptr;
            if(compare(childType, "group")) {
                newItem = new NGWWebMapGroup(m_connection);
            }
            else if(compare(childType, "layer")) {
                newItem = new NGWWebMapLayer(m_connection);
            }

            if(newItem) {
                if(newItem->fill(child)) {
                    m_children.emplace_back(newItem);
                }
                else {
                    delete newItem;
                }
            }
        }
    }
    return true;
}

bool NGWWebMapGroup::deleteItem(intptr_t id)
{
    auto it = m_children.begin();
    while(it != m_children.end()) {
        auto item = (*it);
        if(item->m_id == id) {
            m_children.erase(it);
            return true;
        }
        if(item->m_type == NGWWebMapItem::ItemType::ROOT ||
           item->m_type == NGWWebMapItem::ItemType::GROUP) {
           auto group = ngsDynamicCast(NGWWebMapGroup, item);
           if(group && group->deleteItem(id)) {
               return true;
           }
        }
        ++it;
    }
    return false;
}

intptr_t NGWWebMapGroup::insetItem(intptr_t pos, NGWWebMapItem *item)
{
    if(nullptr == item) {
        return NOT_FOUND;
    }

    if(pos == NOT_FOUND) {
        auto newItem = NGWWebMapItemPtr(item->clone());
        m_children.emplace_back(newItem);
        return newItem->m_id;
    }

    auto it = m_children.begin();
    while(it != m_children.end()) {
        auto currentItem = (*it);
        if(currentItem->m_id == pos) {
            auto newItem = NGWWebMapItemPtr(item->clone());
            if(currentItem->m_type == NGWWebMapItem::ItemType::LAYER) {
                m_children.insert(it, newItem);
            }
            else {
                auto group = ngsDynamicCast(NGWWebMapGroup, currentItem);
                group->m_children.emplace_back(newItem);
            }
            return newItem->m_id;
        }

        if(item->m_type == NGWWebMapItem::ItemType::ROOT ||
           item->m_type == NGWWebMapItem::ItemType::GROUP) {
           auto group = ngsDynamicCast(NGWWebMapGroup, currentItem);
           if(group) {
               auto newId = group->insetItem(pos, item);
               if(newId != NOT_FOUND) {
                   return newId;
               }
           }
        }
        ++it;
    }
    return NOT_FOUND;
}

//------------------------------------------------------------------------------
// NGWWebMapLayer
//------------------------------------------------------------------------------
NGWWebMapLayer::NGWWebMapLayer(NGWConnectionBase *connection) :
    NGWWebMapItem(connection)
{
    m_type = NGWWebMapItem::ItemType::LAYER;
}

CPLJSONObject NGWWebMapLayer::asJson() const
{
    CPLJSONObject out;
    out.Set("layer_adapter", m_adapter);
    out.Set("layer_enabled", m_enabled);
    out.Set("draw_order_position", m_orderPosition);
    if(m_maxScaleDenom.empty()) {
        out.SetNull("layer_max_scale_denom");
    }
    else {
        out.Set("layer_max_scale_denom", m_maxScaleDenom);
    }
    if(m_minScaleDenom.empty()) {
        out.SetNull("layer_min_scale_denom");
    }
    else {
        out.Set("layer_min_scale_denom", m_maxScaleDenom);
    }
    out.Set("item_type", "layer");
    out.Set("display_name", m_displayName);
    auto resource = ngsDynamicCast(NGWResourceBase, m_resource);
    if(resource) {
        out.Set("layer_style_id", atoi(resource->resourceId().c_str()));
    }
    out.Set("layer_transparency", m_transparency);
    return out;
}

bool NGWWebMapLayer::fill(const CPLJSONObject &item)
{
    m_adapter = item.GetString("layer_adapter", "image");
    m_enabled = item.GetBool("layer_enabled", false);
    m_orderPosition = item.GetInteger("draw_order_position", 0);
    m_maxScaleDenom = item.GetString("layer_max_scale_denom");
    m_minScaleDenom = item.GetString("layer_min_scale_denom");
    m_displayName = item.GetString("display_name");

    auto conn = dynamic_cast<NGWResourceGroup*>(m_connection);
    if(conn) {
        auto resource = conn->getResource(item.GetString("layer_style_id"));
        m_resource = resource;
    }

    m_transparency = item.GetInteger("layer_transparency");
    return true;
}

NGWWebMapItem *NGWWebMapLayer::clone()
{
    return new NGWWebMapLayer(*this);
}

//------------------------------------------------------------------------------
// NGWWebMapRoot
//------------------------------------------------------------------------------
NGWWebMapRoot::NGWWebMapRoot(NGWConnectionBase *connection) :
    NGWWebMapGroup(connection)
{
    m_type = NGWWebMapItem::ItemType::ROOT;
    m_expanded = true;
}



} // namespace ngs
