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

#include "api_priv.h"
#include "ngw.h"
#include "util/error.h"
#include "util/stringutil.h"
#include "util/settings.h"
#include "util/url.h"

namespace ngs {

// API
namespace ngw {

std::string getPermisionsUrl(const std::string &url,
                             const std::string &resourceId)
{
    return url + "/api/resource/" + resourceId + "/permission";
}

std::string getResourceUrl(const std::string &url,
                           const std::string &resourceId)
{
    return url + "/api/resource/" + resourceId;
}

std::string getChildrenUrl(const std::string &url,
                           const std::string &resourceId)
{
    return url + "/api/resource/?parent=" + resourceId;
}

std::string getRouteUrl(const std::string &url)
{
    return url + "/api/component/pyramid/route";
}

std::string getSchemaUrl(const std::string &url)
{
    return url + "/resource/schema";
}

std::string getCurrentUserUrl(const std::string &url)
{
    return url + "/api/component/auth/current_user";
}

std::string getTrackerUrl()
{
    std::string trackAPIEndpoint = Settings::instance().getString("nextgis/track_api", "track.nextgis.com");
    return trackAPIEndpoint + "/ng-mobile/" + deviceId(false);
}

std::string getUploadUrl(const std::string &url)
{
    return url + "/api/component/file_upload/upload";
}

std::string getTMSUrl(const std::string &url,
                      const std::vector<std::string> &resourceIds)
{
    auto out = url + "/api/component/render/tile?z=${z}&amp;x=${x}&amp;y=${y}&amp;resource=";
    bool isFirst = true;
    for(const auto &id : resourceIds) {
        if(isFirst) {
            isFirst = false;
        }
        else {
            out += ",";
        }
        out += id;
    }
    return out;
}

std::string objectTypeToNGWClsType(enum ngsCatalogObjectType type)
{
    switch(type) {
    case CAT_NGW_GROUP:
        return "resource_group";
    case CAT_NGW_TRACKERGROUP:
        return "trackers_group";
    case CAT_NGW_TRACKER:
        return "tracker";
    case CAT_NGW_POSTGIS_CONNECTION:
        return "postgis_connection";
    case CAT_NGW_VECTOR_LAYER:
        return "vector_layer";
    case CAT_NGW_POSTGIS_LAYER:
        return "postgis_layer";
    case CAT_NGW_BASEMAP:
        return "basemap_layer";
    case CAT_NGW_RASTER_LAYER:
        return "raster_layer";
    case CAT_NGW_MAPSERVER_STYLE:
        return "mapserver_style";
    case CAT_NGW_QGISRASTER_STYLE:
        return "qgis_raster_style";
    case CAT_NGW_QGISVECTOR_STYLE:
        return "qgis_vector_style";
    case CAT_NGW_RASTER_STYLE:
        return "raster_style";
    case CAT_NGW_WMS_CONNECTION:
        return "wmsclient_connection";
    case CAT_NGW_WMS_LAYER:
        return "wmsclient_layer";
    case CAT_NGW_WMS_SERVICE:
        return "wmsserver_service";
    case CAT_NGW_WFS_SERVICE:
        return "wfsserver_service";
    case CAT_NGW_LOOKUP_TABLE:
        return "lookup_table";
    case CAT_NGW_WEBMAP:
        return "webmap";
    case CAT_NGW_FORMBUILDER_FORM:
        return "formbuilder_form";
    case CAT_NGW_FILE_BUCKET:
        return "file_bucket";
    default:
        return "resource";
    }
}

bool checkVersion(const std::string &version, int major, int minor, int patch)
{
    int currentMajor(0);
    int currentMinor(0);
    int currentPatch(0);

    CPLStringList versionPartsList(CSLTokenizeString2(version.c_str(), ".", 0));
    if(versionPartsList.size() > 2) {
        currentMajor = atoi(versionPartsList[0]);
        currentMinor = atoi(versionPartsList[1]);
        currentPatch = atoi(versionPartsList[2]);
    }
    else if(versionPartsList.size() > 1) {
        currentMajor = atoi(versionPartsList[0]);
        currentMinor = atoi(versionPartsList[1]);
    }
    else if(versionPartsList.size() > 0) {
        currentMajor = atoi(versionPartsList[0]);
    }

    return currentMajor >= major && currentMinor >= minor && currentPatch >= patch;
}

static void reportError(const GByte *pabyData, int nDataLen)
{
    CPLJSONDocument result;
    if(result.LoadMemory(pabyData, nDataLen)) {
        CPLJSONObject root = result.GetRoot();
        if(root.IsValid()) {
            std::string errorMessageStr = root.GetString("message");
            if(!errorMessageStr.empty()) {
                errorMessage("%s", errorMessageStr.c_str());
                return;
            }
        }
    }

    errorMessage(_("Unexpected error occurred."));
}

bool sendTrackPoints(const std::string &payload)
{
    CPLErrorReset();
    std::string payloadInt = "POSTFIELDS=" + payload;
    char **httpOptions = nullptr;
    httpOptions = CSLAddString(httpOptions, "CUSTOMREQUEST=POST");
    httpOptions = CSLAddString(httpOptions, payloadInt.c_str());
    httpOptions = CSLAddString(httpOptions,
            "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    std::string url = ngw::getTrackerUrl() + "/packet";

    http::HTTPResultPtr result = CPLHTTPFetch(url.c_str(), httpOptions);
    CSLDestroy(httpOptions);

    bool outResult = false;
    if(result) {
        outResult = result->nStatus == 0 && result->pszErrBuf == nullptr;

        // Get error message.
        if(!outResult) {
            reportError(result->pabyData, result->nDataLen);
        }
    }

    return outResult;
}

std::string createResource(const std::string &url, const std::string &payload,
                           char **httpOptions)
{
    resetError();
    std::string payloadInt = "POSTFIELDS=" + payload;

    httpOptions = CSLAddString(httpOptions, "CUSTOMREQUEST=POST");
    httpOptions = CSLAddString(httpOptions, payloadInt.c_str());
    httpOptions = CSLAddString(httpOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*");

    CPLJSONDocument createReq;
    bool bResult = createReq.LoadUrl(getResourceUrl(url, ""), httpOptions);
    CSLDestroy(httpOptions);
    std::string resourceId("-1");
    CPLJSONObject root = createReq.GetRoot();
    if(root.IsValid()) {
        if(bResult) {
            resourceId = root.GetString("id", "-1");
        }
        else {
            std::string errorMessageStr = root.GetString("message");
            if(errorMessageStr.empty()) {
                errorMessage("%s", _("Create resource failed. No error message from server."));
            }
            else {
                errorMessage("%s", errorMessageStr.c_str());
            }
        }
    }
    return resourceId;
}

bool updateResource(const std::string &url, const std::string &resourceId,
    const std::string &payload, char **httpOptions)
{
    CPLErrorReset();
    std::string payloadInt = "POSTFIELDS=" + payload;

    httpOptions = CSLAddString( httpOptions, "CUSTOMREQUEST=PUT" );
    httpOptions = CSLAddString( httpOptions, payloadInt.c_str() );
    httpOptions = CSLAddString( httpOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    http::HTTPResultPtr httpResult = CPLHTTPFetch(
                getResourceUrl(url, resourceId).c_str(), httpOptions);
    CSLDestroy(httpOptions);
    bool result = false;
    if(httpResult) {
        result = httpResult->nStatus == 0 && httpResult->pszErrBuf == nullptr;

        // Get error message.
        if(!result) {
            reportError(httpResult->pabyData, httpResult->nDataLen);
        }
    }
    else {
        errorMessage(_("Update resource %s failed"), resourceId.c_str());
    }
    return result;
}

bool deleteResource(const std::string &url, const std::string &resourceId,
    char **httpOptions)
{
    CPLErrorReset();
    httpOptions = CSLAddString(httpOptions, "CUSTOMREQUEST=DELETE");
    http::HTTPResultPtr httpResult = CPLHTTPFetch(
                getResourceUrl(url, resourceId).c_str(), httpOptions);
    CSLDestroy(httpOptions);
    bool result = false;
    if(httpResult) {
        result = httpResult->nStatus == 0 && httpResult->pszErrBuf == nullptr;
        // Get error message.
        if(!result) {
            reportError(httpResult->pabyData, httpResult->nDataLen);
        }
    }
    return result;
}


bool deleteAttachment(const std::string &url, const std::string &resourceId,
                      const std::string &featureId,
                      const std::string &attachmentId,
                      char **httpOptions)
{
    CPLErrorReset();
    httpOptions = CSLAddString(httpOptions, "CUSTOMREQUEST=DELETE");
    http::HTTPResultPtr httpResult =
            CPLHTTPFetch(getAttachmentUrl(url, resourceId, featureId,
                                          attachmentId).c_str(), httpOptions);
    CSLDestroy(httpOptions);
    bool result = false;
    if(httpResult) {
        result = httpResult->nStatus == 0 && httpResult->pszErrBuf == nullptr;
        // Get error message.
        if(!result) {
            reportError(httpResult->pabyData, httpResult->nDataLen);
        }
    }
    return result;
}


bool updateFeature(const std::string &url, const std::string &resourceId,
                        const std::string &featureId, const std::string &payload,
                        char **httpOptions)
{
    auto featureUrl = getFeatureUrl(url, resourceId, featureId);
    CPLErrorReset();
    std::string payloadInt = "POSTFIELDS=" + payload;

    httpOptions = CSLAddString( httpOptions, "CUSTOMREQUEST=PUT" );
    httpOptions = CSLAddString( httpOptions, payloadInt.c_str() );
    httpOptions = CSLAddString( httpOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

    http::HTTPResultPtr httpResult = CPLHTTPFetch(featureUrl.c_str(), httpOptions);
    CSLDestroy(httpOptions);
    bool result = false;
    if(httpResult) {
        result = httpResult->nStatus == 0 && httpResult->pszErrBuf == nullptr;

        // Get error message.
        if(!result) {
            reportError(httpResult->pabyData, httpResult->nDataLen);
        }
    }
    else {
        errorMessage(_("Update feature %s in feature class %s failed"),
                     featureId.c_str(), resourceId.c_str());
    }
    return result;
}

bool deleteAttachments(const std::string &url, const std::string &resourceId,
                       const std::string &featureId, char **httpOptions)
{
    return updateFeature(url, resourceId, featureId,
                         "{\"extensions\":{\"attachment\":[]}}", httpOptions);
}

bool renameResource(const std::string &url, const std::string &resourceId,
    const std::string &newName, char **httpOptions)
{
    CPLErrorReset();
    CPLJSONObject payload;
    CPLJSONObject resource("resource", payload);
    resource.Add("display_name", newName);
    std::string payloadStr = payload.Format(CPLJSONObject::PrettyFormat::Plain);

    return updateResource( url, resourceId, payloadStr, httpOptions);
}

std::string resmetaSuffix(CPLJSONObject::Type eType)
{
    switch( eType ) {
        case CPLJSONObject::Type::Integer:
        case CPLJSONObject::Type::Long:
            return ".d";
        case CPLJSONObject::Type::Double:
            return ".f";
        default:
            return "";
    }
}

std::string getFeatureUrl(const std::string &url,
                          const std::string &resourceId,
                          const std::string &featureId)
{
    return getResourceUrl(url, resourceId) + "/feature/" + featureId;
}

std::string getAttachmentUrl(const std::string &url,
                                  const std::string &resourceId,
                                  const std::string &featureId,
                                  const std::string &attachmentId)
{
    return getAttachmentCreateUrl(url, resourceId, featureId) + attachmentId;
}

std::string getAttachmentCreateUrl(const std::string &url,
                                   const std::string &resourceId,
                                   const std::string &featureId)
 {
     return getFeatureUrl(url, resourceId, featureId) + "/attachment/";
 }


std::string getAttachmentDownloadUrl(const std::string &url,
                                     const std::string &resourceId,
                                     const std::string &featureId,
                                     const std::string &attachmentId)
{
    return getAttachmentUrl(url, resourceId, featureId, attachmentId) + "/download";
}

GIntBig addAttachment(const std::string &url,
                          const std::string &resourceId,
                          const std::string &featureId,
                          const std::string &payload,
                          char **httpOptions)
{
    resetError();
    std::string payloadInt = "POSTFIELDS=" + payload;

    httpOptions = CSLAddString(httpOptions, "CUSTOMREQUEST=POST");
    httpOptions = CSLAddString(httpOptions, payloadInt.c_str());
    httpOptions = CSLAddString(httpOptions,
        "HEADERS=Content-Type: application/json\r\nAccept: */*");

    CPLJSONDocument createReq;
    bool bResult = createReq.LoadUrl(
                getAttachmentCreateUrl(url, resourceId, featureId), httpOptions);
    CSLDestroy(httpOptions);
    GIntBig attachmentId(-1);
    CPLJSONObject root = createReq.GetRoot();
    if(root.IsValid()) {
        if(bResult) {
            attachmentId = root.GetLong("id", -1);
        }
        else {
            std::string errorMessageStr = root.GetString("message");
            if(errorMessageStr.empty()) {
                errorMessage("%s", _("Create attachment failed. No error message from server."));
            }
            else {
                errorMessage("%s", errorMessageStr.c_str());
            }
        }
    }
    return attachmentId;
}

//// C++11 allow defaults
//struct Permissions {
//    bool bResourceCanRead = false;
//    bool bResourceCanCreate = false;
//    bool bResourceCanUpdate = false;
//    bool bResourceCanDelete = false;
//    bool bDatastructCanRead = false;
//    bool bDatastructCanWrite = false;
//    bool bDataCanRead = false;
//    bool bDataCanWrite = false;
//    bool bMetadataCanRead = false;
//    bool bMetadataCanWrite = false;
//};

//static Permissions checkPermissions(const std::string &url,
//    const std::string &resourceId, char **papszHTTPOptions, bool readWrite)
//{
//    Permissions out;
//    CPLErrorReset();
//    CPLJSONDocument permissionReq;
//    bool bResult = permissionReq.LoadUrl( getPermisionsUrl( url, resourceId ),
//        papszHTTPOptions );

//    CPLJSONObject root = permissionReq.GetRoot();
//    if( root.IsValid() )
//    {
//        if( bResult )
//        {
//            out.bResourceCanRead = root.GetBool( "resource/read", true );
//            out.bResourceCanCreate = root.GetBool( "resource/create", readWrite );
//            out.bResourceCanUpdate = root.GetBool( "resource/update", readWrite );
//            out.bResourceCanDelete = root.GetBool( "resource/delete", readWrite );

//            out.bDatastructCanRead = root.GetBool( "datastruct/read", true );
//            out.bDatastructCanWrite = root.GetBool( "datastruct/write", readWrite );

//            out.bDataCanRead = root.GetBool( "data/read", true );
//            out.bDataCanWrite = root.GetBool( "data/write", readWrite );

//            out.bMetadataCanRead = root.GetBool( "metadata/read", true );
//            out.bMetadataCanWrite = root.GetBool( "metadata/write", readWrite );
//        }
//        else
//        {
//            std::string osErrorMessage = root.GetString("message");
//            if( osErrorMessage.empty() )
//            {
//                osErrorMessage = "Get permissions failed";
//            }
//            CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrorMessage.c_str());
//        }
//    }
//    else
//    {
//        CPLError(CE_Failure, CPLE_AppDefined, "Get permissions failed");
//    }

//    return out;
//}

//static void fillResmeta(CPLJSONObject &root, char **papszMetadata)
//{
//    CPLJSONObject resMeta("resmeta", root);
//    CPLJSONObject resMetaItems("items", resMeta);
//    CPLStringList metadata(papszMetadata, FALSE);
//    for( int i = 0; i < metadata.size(); ++i )
//    {
//        std::string item = metadata[i];
//        size_t pos = item.find("=");
//        if( pos != std::string::npos )
//        {
//            std::string itemName = item.substr( 0, pos );
//            CPLString itemValue = item.substr( pos + 1 );

//            if( itemName.size() > 2 )
//            {
//                size_t suffixPos = itemName.size() - 2;
//                std::string osSuffix = itemName.substr(suffixPos);
//                if( osSuffix == ".d")
//                {
//                    GInt64 nVal = CPLAtoGIntBig( itemValue.c_str() );
//                    resMetaItems.Add( itemName.substr(0, suffixPos), nVal );
//                    continue;
//                }

//                if( osSuffix == ".f")
//                {
//                    resMetaItems.Add( itemName.substr(0, suffixPos),
//                        CPLAtofM(itemValue.c_str()) );
//                    continue;
//                }
//            }

//            resMetaItems.Add(itemName, itemValue);
//        }
//    }
//}

//static bool flushMetadata(const std::string &url, const std::string &resourceId,
//    char **papszMetadata, char **papszHTTPOptions )
//{
//    if( nullptr == papszMetadata )
//    {
//        return true;
//    }
//    CPLJSONObject metadataJson;
//    fillResmeta(metadataJson, papszMetadata);

//    return updateResource(url, resourceId,
//                    metadataJson.Format(CPLJSONObject::Plain), papszHTTPOptions);
//}

} // namespace ngw

} // namespace ngs
