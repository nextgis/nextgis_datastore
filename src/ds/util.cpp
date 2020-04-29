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

#include "util.h"

#include "catalog/catalog.h"
#include "catalog/file.h"
#include "catalog/folder.h"
#include "catalog/ngw.h"
#include "util/error.h"
#include "util/url.h"

namespace ngs {

CREATE_FEATURE_DEFN_RESULT createFeatureDefinition(const std::string& name,
                                        const Options &options)
{
    CREATE_FEATURE_DEFN_RESULT out;
    OGRFeatureDefn *fieldDefinition = OGRFeatureDefn::CreateFeatureDefn(name.c_str());
    int fieldCount = options.asInt("FIELD_COUNT", 0);

    for(int i = 0; i < fieldCount; ++i) {
        std::string fieldName = options.asString(CPLSPrintf("FIELD_%d_NAME", i), "");
        if(fieldName.empty()) {
            OGRFeatureDefn::DestroyFeatureDefn(fieldDefinition);
            errorMessage(_("Name for field %d is not defined"), i);
            return out;
        }

        std::string fieldAlias = options.asString(CPLSPrintf("FIELD_%d_ALIAS", i), "");
        if(fieldAlias.empty()) {
            fieldAlias = fieldName;
        }
        fieldData data = { fieldName, fieldAlias };
        out.fields.push_back(data);

        OGRFieldType fieldType = FeatureClass::fieldTypeFromName(
                    options.asString(CPLSPrintf("FIELD_%d_TYPE", i), ""));
        OGRFieldDefn field(fieldName.c_str(), fieldType);
        std::string defaultValue = options.asString(
                    CPLSPrintf("FIELD_%d_DEFAULT_VAL", i), "");
        if(!defaultValue.empty()) {
            field.SetDefault(defaultValue.c_str());
        }
        fieldDefinition->AddFieldDefn(&field);
    }
    out.defn = std::unique_ptr<OGRFeatureDefn>(fieldDefinition);
    return out;
}

void setMetadata(const ObjectPtr &object, std::vector<fieldData> fields,
                 const Options& options)
{
    Table *table = ngsDynamicCast(Table, object);
    if(nullptr == table) {
        return;
    }

    // Store aliases and field original names in properties
    for(size_t i = 0; i < fields.size(); ++i) {
        table->setProperty("FIELD_" + std::to_string(i) + "_NAME",
            fields[i].name, NG_ADDITIONS_KEY);
        table->setProperty("FIELD_" + std::to_string(i) + "_ALIAS",
            fields[i].alias, NG_ADDITIONS_KEY);
    }

    bool saveEditHistory = options.asBool(LOG_EDIT_HISTORY_KEY, false);
    table->setProperty(LOG_EDIT_HISTORY_KEY, saveEditHistory ? "ON" : "OFF",
        NG_ADDITIONS_KEY);

    // Store user defined options in properties
    for(const auto &it : options) {
        if(comparePart(it.first, USER_PREFIX_KEY, USER_PREFIX_KEY_LEN)) {
            table->setProperty(it.first.substr(USER_PREFIX_KEY_LEN),
                it.second, USER_KEY);
        }
    }
}

namespace ngw {

OGRLayer *createAttachmentsTable(GDALDataset *ds, const std::string &name)
{
    resetError();
    OGRLayer *attLayer = ds->CreateLayer(name.c_str(), nullptr, wkbNone, nullptr);
    if (nullptr == attLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create table  fields
    OGRFieldDefn fidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn nameField(ATTACH_FILE_NAME_FIELD, OFTString);
    OGRFieldDefn descField(ATTACH_DESCRIPTION_FIELD, OFTString);
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));

    if(attLayer->CreateField(&fidField) != OGRERR_NONE ||
       attLayer->CreateField(&nameField) != OGRERR_NONE ||
       attLayer->CreateField(&descField) != OGRERR_NONE ||
       attLayer->CreateField(&ridField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return attLayer;
}

OGRLayer *createEditHistoryTable(GDALDataset *ds, const std::string &name)
{
    resetError();
    OGRLayer *logLayer = ds->CreateLayer(name.c_str(), nullptr, wkbNone, nullptr);
    if (nullptr == logLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create table  fields
    OGRFieldDefn fidField(FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn afidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn opField(OPERATION_FIELD, OFTInteger64);
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));
    OGRFieldDefn aridField(ATTACHMENT_REMOTE_ID_KEY, OFTInteger64);
    aridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));

    if(logLayer->CreateField(&fidField) != OGRERR_NONE ||
       logLayer->CreateField(&afidField) != OGRERR_NONE ||
       logLayer->CreateField(&opField) != OGRERR_NONE ||
       logLayer->CreateField(&ridField) != OGRERR_NONE ||
       logLayer->CreateField(&aridField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return logLayer;
}

std::string downloadAttachment(StoreObject *storeObject, GIntBig fid, GIntBig aid,
                               const Progress &progress)
{
    if(nullptr == storeObject) {
        return "";
    }

    auto table = dynamic_cast<Table*>(storeObject);
    if(nullptr == table) {
        return "";
    }

    resetError();
    auto connectionPath = table->property(NGW_CONNECTION, "", NG_ADDITIONS_KEY);
    if(connectionPath.empty()) {
        warningMessage(_("No remote NextGIS Web connection defined in feature class properties"));
        return "";
    }
    auto catalog = Catalog::instance();
    auto ngwConnection = catalog->getObject(connectionPath);
    if(nullptr == ngwConnection) {
        warningMessage(_("Connection to NextGIS Web '%s' defined in feature class properties is not present."),
                       connectionPath.c_str());
        return "";
    }

    auto conn = ngsDynamicCast(NGWConnection, ngwConnection);
    if(nullptr == conn) {
        warningMessage(_("Connection to NextGIS Web '%s' defined in feature class properties is not valid."),
                       connectionPath.c_str());
        return "";
    }
    conn->fillProperties();
    auto url = conn->connectionUrl();

    auto resourceId = table->property(NGW_ID, "", NG_ADDITIONS_KEY);
    auto rid = storeObject->getRemoteId(fid);
    auto arid = storeObject->getAttachmentRemoteId(aid);

    auto attachmentUrl = ngw::getAttachmentDownloadUrl(url, resourceId,
                                    std::to_string(rid), std::to_string(arid));

    auto dstPath = table->getAttachmentPath(fid, aid, true);
    if(http::getFile(url, dstPath, progress)) {
        return dstPath;
    }
    return "";
}

} // namespace ngw

namespace qms {

constexpr const char *qmsAPIURL = "https://qms.nextgis.com/api/v1/";

static enum ngsCode qmsStatusToCode(const std::string &status)
{
    if(status.empty()) {
        return COD_REQUEST_FAILED;
    }

    if(EQUAL(status.c_str(), "works")) {
        return COD_SUCCESS;
    }

    if(EQUAL(status.c_str(), "problematic")) {
        return COD_WARNING;
    }

    return COD_REQUEST_FAILED;
}

static enum ngsCatalogObjectType qmsTypeToCode(const std::string &type)
{
    if(type.empty()) {
        return CAT_UNKNOWN;
    }

    if(EQUAL(type.c_str(), "tms")) {
        return CAT_RASTER_TMS;
    }

    if(EQUAL(type.c_str(), "wms")) {
        return CAT_CONTAINER_WMS;
    }

    if(EQUAL(type.c_str(), "wfs")) {
        return CAT_CONTAINER_WFS;
    }

    if(EQUAL(type.c_str(), "geojson")) {
        return CAT_FC_GEOJSON;
    }

    return CAT_UNKNOWN;
}

static Envelope qmsExtentToEnvelope(const std::string &extent)
{
    Envelope out(DEFAULT_BOUNDS);
    if(extent.empty() || extent.size() < 11) {
        return out;
    }
    // Drop SRID part - 10 chars
    std::string wkt = extent.substr(10);
    OGRGeometry *poGeom = nullptr;
    if(OGRGeometryFactory::createFromWkt(wkt.c_str(),
        OGRSpatialReference::GetWGS84SRS(), &poGeom) != OGRERR_NONE ) {
        SpatialReferencePtr srsWebMercator = SpatialReferencePtr::importFromEPSG(3857);
        if(poGeom->transformTo(srsWebMercator) != OGRERR_NONE) {
            OGREnvelope env;
            poGeom->getEnvelope(&env);
            out.setMinX(env.MinX);
            out.setMinY(env.MinY);
            out.setMaxX(env.MaxX);
            out.setMaxY(env.MaxY);
        }
    }
    return out;
}

std::vector<Item> QMSQuery(const Options &options)
{
    std::string url(qmsAPIURL);
    url.append("geoservices/");
    bool firstParam = true;

    std::string val = options.asString("type");
    if(!val.empty()) {
        url.append("?type=" + val);
        firstParam = false;
    }

    val = options.asString("epsg");
    if(!val.empty()) {
        if(firstParam) {
            url.append("?epsg=" + val);
            firstParam = false;
        }
        else {
            url.append("&epsg=" + val);
        }
    }

    val = options.asString("cumulative_status");
    if(!val.empty()) {
        if(firstParam) {
            url.append("?cumulative_status=" + val);
            firstParam = false;
        }
        else {
            url.append("&cumulative_status=" + val);
        }
    }

    val = options.asString("search");
    if(!val.empty()) {
        if(firstParam) {
            url.append("?search=" + val);
            firstParam = false;
        }
        else {
            url.append("&search=" + val);
        }
    }

    val = options.asString("intersects_extent");
    if(!val.empty()) {
        if(firstParam) {
            url.append("?intersects_extent=" + val);
            firstParam = false;
        }
        else {
            url.append("&intersects_extent=" + val);
        }
    }

    val = options.asString("ordering");
    if(!val.empty()) {
        if(firstParam) {
            url.append("?ordering=" + val);
            firstParam = false;
        }
        else {
            url.append("&ordering=" + val);
        }
    }

    val = options.asString("limit", "20");
    if(firstParam) {
        url.append("?limit=" + val);
        firstParam = false;
    }
    else {
        url.append("&limit=" + val);
    }

    val = options.asString("offset", "0");
    if(firstParam) {
        url.append("?offset=" + val);
    }
    else {
        url.append("&offset=" + val);
    }

    Envelope ext;
    std::vector<Item> out;
    CPLJSONObject root = http::fetchJson(url);
    if(root.IsValid()) {
        CPLJSONArray services = root.GetArray("results");
        for(int i = 0; i < services.Size(); ++i) {
            CPLJSONObject service = services[i];
            int iconId = service.GetInteger("icon", -1);
            std::string iconUrl;
            if(iconId != -1) {
                iconUrl = std::string(qmsAPIURL) + "icons/" +
                        std::to_string(iconId) + "/content";
            }

            Item item = { service.GetInteger("id"),
                          service.GetString("name"),
                          service.GetString("desc"),
                          qmsTypeToCode(service.GetString("type")),
                          iconUrl,
                          qmsStatusToCode(service.GetString("status")),
                          qmsExtentToEnvelope(service.GetString("extent"))
                     };
            out.emplace_back(item);
        }
    }

    return out;
}

CPLJSONObject QMSItemProperties(int id)
{
    return http::fetchJson(std::string(qmsAPIURL) + "geoservices/" +
                           std::to_string(id));
}

ItemProperties QMSQueryProperties(int id)
{
    ItemProperties out;
    out.id = NOT_FOUND;
    auto jsonProp = QMSItemProperties(id);
    if(jsonProp.IsValid()) {
        out.id = id;
        out.status = qmsStatusToCode(jsonProp.GetString("cumulative_status", "failed"));
        out.url = jsonProp.GetString("url");
        out.name = jsonProp.GetString("name");
        out.desc = jsonProp.GetString("desc");
        out.type = qmsTypeToCode(jsonProp.GetString("type"));
        out.epsg = jsonProp.GetInteger("epsg", 3857);
        out.z_min = jsonProp.GetInteger("z_min", 0);
        out.z_max = jsonProp.GetInteger("z_max", 20);
        int iconId = jsonProp.GetInteger("icon", NOT_FOUND);
        if(iconId != NOT_FOUND) {
            out.iconUrl = std::string(qmsAPIURL) + "icons/" +
                    std::to_string(iconId) + "/content";
        }
        out.y_origin_top = jsonProp.GetBool("y_origin_top");
        out.extent = qmsExtentToEnvelope(jsonProp.GetString("extent"));
    }

    return out;
}

} // namespace qms

} // namespace ngs
