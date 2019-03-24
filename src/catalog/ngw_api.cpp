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
#include "api_priv.h"
#include "ngw.h"
#include "util/error.h"
#include "util/stringutil.h"

#include "cpl_http.h"

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

std::string getCurrentUserUrl(const std::string &url)
{
    return url + "/api/component/auth/current_user";
}

std::string getTrackerUrl()
{
    return "track.nextgis.com/ng-mobile/" + deviceId(false); //std::string("971f1-ffc-0f7073");//
}

std::string objectTypeToNGWClsType(enum ngsCatalogObjectType type)
{
    switch(type) {
    case CAT_CONTAINER_NGWGROUP:
        return "resource_group";
    case CAT_CONTAINER_NGWTRACKERGROUP:
        return "trackers_group";
    case CAT_NGW_TRACKER:
        return "tracker";
    default:
        return "";
    }
}

bool checkVersion(const std::string &version, int major, int minor, int patch)
{
    int currentMajor(0);
    int currentMinor(0);
    int currentPatch(0);

    CPLStringList versionPartsList(CSLTokenizeString2(version.c_str(), ".", 0));
    if(versionPartsList.size() > 2)
    {
        currentMajor = atoi(versionPartsList[0]);
        currentMinor = atoi(versionPartsList[1]);
        currentPatch = atoi(versionPartsList[2]);
    }
    else if(versionPartsList.size() > 1)
    {
        currentMajor = atoi(versionPartsList[0]);
        currentMinor = atoi(versionPartsList[1]);
    }
    else if(versionPartsList.size() > 0)
    {
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

    CPLHTTPResult *result = CPLHTTPFetch(url.c_str(), httpOptions);
    CSLDestroy(httpOptions);

    bool outResult = false;
    if(result) {
        outResult = result->nStatus == 0 && result->pszErrBuf == nullptr;

        // Get error message.
        if(!outResult) {
            reportError(result->pabyData, result->nDataLen);
        }
        CPLHTTPDestroyResult(result);
    }

    return outResult;
}

std::string createResource(const std::string &url, const std::string &payload,
                           char **httpOptions)
{
    CPLErrorReset();
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
            if(!errorMessageStr.empty()) {
                errorMessage("%s", errorMessageStr.c_str());
            }
        }
    }
    return resourceId;
}

//static bool updateResource(const std::string &url, const std::string &resourceId,
//    const std::string &payload, char **papszHTTPOptions)
//{
//    CPLErrorReset();
//    std::string payloadInt = "POSTFIELDS=" + payload;

//    papszHTTPOptions = CSLAddString( papszHTTPOptions, "CUSTOMREQUEST=PUT" );
//    papszHTTPOptions = CSLAddString( papszHTTPOptions, payloadInt.c_str() );
//    papszHTTPOptions = CSLAddString( papszHTTPOptions,
//        "HEADERS=Content-Type: application/json\r\nAccept: */*" );

//    CPLDebug("NGW", "UpdateResource request payload: %s", payload.c_str());

//    CPLHTTPResult *psResult = CPLHTTPFetch( getResourceUrl(url, resourceId).c_str(),
//        papszHTTPOptions );
//    CSLDestroy( papszHTTPOptions );
//    bool bResult = false;
//    if( psResult )
//    {
//        bResult = psResult->nStatus == 0 && psResult->pszErrBuf == nullptr;

//        // Get error message.
//        if( !bResult )
//        {
//            reportError(psResult->pabyData, psResult->nDataLen);
//        }
//        CPLHTTPDestroyResult(psResult);
//    }
//    else
//    {
//        CPLError(CE_Failure, CPLE_AppDefined, "Update resource %s failed",
//            resourceId.c_str());
//    }
//    return bResult;
//}

bool deleteResource(const std::string &url, const std::string &resourceId,
    char **httpOptions)
{
    CPLErrorReset();
    httpOptions = CSLAddString(httpOptions, "CUSTOMREQUEST=DELETE");
    CPLHTTPResult *httpResult = CPLHTTPFetch(getResourceUrl(url, resourceId).c_str(),
        httpOptions);
    bool result = false;
    if(httpResult) {
        result = httpResult->nStatus == 0 && httpResult->pszErrBuf == nullptr;
        // Get error message.
        if(!result) {
            reportError(httpResult->pabyData, httpResult->nDataLen);
        }
        CPLHTTPDestroyResult(httpResult);
    }
    CSLDestroy(httpOptions);
    return result;
}

//static bool renameResource(const std::string &url, const std::string &resourceId,
//    const std::string &newName, char **papszHTTPOptions)
//{
//    CPLJSONObject payload;
//    CPLJSONObject resource("resource", payload);
//    resource.Add("display_name", newName);
//    std::string payloadStr = payload.Format(CPLJSONObject::Plain);

//    return updateResource( url, resourceId, payloadStr, papszHTTPOptions);
//}

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

//static std::string getResmetaSuffix(CPLJSONObject::Type type)
//{
//    switch( type ) {
//        case CPLJSONObject::Integer:
//        case CPLJSONObject::Long:
//            return ".d";
//        case CPLJSONObject::Double:
//            return ".f";
//        default:
//            return "";
//    }
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
