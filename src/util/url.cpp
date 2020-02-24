/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
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
#include "url.h"

#include "authstore.h"
#include "catalog/file.h"
#include "error.h"
#include "stringutil.h"

namespace ngs {

namespace http {

//------------------------------------------------------------------------------
// HTTPResultPtr
//------------------------------------------------------------------------------


HTTPResultPtr::HTTPResultPtr(CPLHTTPResult *result) :
    shared_ptr(result, CPLHTTPDestroyResult)
{

}

HTTPResultPtr::HTTPResultPtr() :
    shared_ptr(nullptr, CPLHTTPDestroyResult)
{

}

HTTPResultPtr &HTTPResultPtr::operator=(CPLHTTPResult *result)
{
    reset(result);
    return *this;
}

HTTPResultPtr::operator CPLHTTPResult *() const
{
     return get();
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

ngsURLRequestResult *fetch(const std::string &url, const Progress &progress,
                           const Options &options)
{
    resetError();
    ngsURLRequestResult *out = new ngsURLRequestResult;
    auto requestOptions = options.asCPLStringList();
    requestOptions = addAuthHeaders(url, requestOptions);

    Progress progressIn(progress);
    HTTPResultPtr result = CPLHTTPFetchEx(url.c_str(), requestOptions,
                                           ngsGDALProgress, &progressIn, nullptr,
                                           nullptr);
    if(nullptr == result) {
        outMessage(COD_REQUEST_FAILED, _("Unexpected error"));
        return nullptr;
    }
    if(result->nStatus != 0 || result->pszErrBuf != nullptr) {
        outMessage(COD_REQUEST_FAILED, result->pszErrBuf);
        out->status = result->nStatus;
        out->headers = nullptr;
        out->dataLen = 0;
        out->data = nullptr;
        return out;
    }

    out->status = result->nStatus;
    out->headers = result->papszHeaders;
    out->dataLen = result->nDataLen;
    out->data = result->pabyData;

    // Transfer own to out, don't delete with result
    result->papszHeaders = nullptr;
    result->pabyData = nullptr;

    return out;
}

CPLJSONObject fetchJson(const std::string &url, const Progress &progress,
                        const Options &options)
{
    CPLJSONDocument doc;
    auto requestOptions = options.asCPLStringList();
    requestOptions = addAuthHeaders(url, requestOptions);
    Progress progressIn(progress);
    if(doc.LoadUrl(url, requestOptions, ngsGDALProgress, &progressIn)) {
        return doc.GetRoot();
    }
    return CPLJSONObject();
}

static size_t ngsWriteFct(void *buffer, size_t size, size_t nmemb, void *reqInfo)
{
    VSILFILE * const fp =  static_cast<VSILFILE * const>(reqInfo);
    size_t nBytesToWrite = nmemb * size;

    if(VSIFWriteL(buffer, nBytesToWrite, 1, fp) != 1) {
        errorMessage(_("Write file failed"));
        return 0;
    }

    return nmemb;
}

bool getFile(const std::string &url, const std::string &path,
             const Progress &progress, const Options &options)
{
    resetError();
    VSILFILE * const fp = VSIFOpenL( path.c_str(), "wb" );
    if( fp == nullptr ) {
        return errorMessage(_("Create file %s failed"), path.c_str());
    }

    auto requestOptions = options.asCPLStringList();
    requestOptions = addAuthHeaders(url, requestOptions);
    Progress progressIn(progress);
    HTTPResultPtr result = CPLHTTPFetchEx(url.c_str(), requestOptions,
                                           ngsGDALProgress, &progressIn,
                                           ngsWriteFct, fp);

    bool ret = VSIFCloseL(fp) == 0;
    if(nullptr == result) {
        outMessage(COD_REQUEST_FAILED, _("Unexpected error"));
        return false;
    }
    if(result->nStatus != 0 || result->pszErrBuf != nullptr) {
        outMessage(COD_REQUEST_FAILED, result->pszErrBuf);
        return false;
    }

    return ret;
}

CPLStringList addAuthHeaders(const std::string &url, CPLStringList &options)
{
    std::string auth = AuthStore::authHeader(url);
    if(!auth.empty()) {
        const char *headers = options.FetchNameValue("HEADERS");
        if(nullptr != headers) {
            auth += "\r\n" + std::string(headers);
            options.SetNameValue("HEADERS", auth.c_str());
        }
        else {
            options.AddNameValue("HEADERS", auth.c_str());
        }
    }
    return options;
}

CPLStringList getGDALHeaders(const std::string &url)
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

CPLJSONObject uploadFile(const std::string &url, const std::string &filePath,
                         const Progress &progress, const Options &options)
{
    resetError();
    auto requestOptions = options.asCPLStringList();
    requestOptions = addAuthHeaders(url, requestOptions);
    requestOptions.AddNameValue("FORM_FILE_PATH", filePath.c_str());
    requestOptions.AddNameValue("FORM_FILE_NAME", "file");
    requestOptions.AddNameValue("FORM_KEY_0", "name");
    requestOptions.AddNameValue("FORM_VALUE_0", File::getFileName(filePath).c_str());
    requestOptions.AddNameValue("FORM_ITEM_COUNT", "1");

    Progress progressIn(progress);

    HTTPResultPtr httpResult = CPLHTTPFetchEx(url.c_str(), requestOptions,
                                           ngsGDALProgress, &progressIn,
                                           nullptr, nullptr);
    if(nullptr == httpResult) {
        outMessage(COD_REQUEST_FAILED, _("Unexpected error"));
        return CPLJSONObject();
    }
    if(httpResult->nStatus != 0 || httpResult->pszErrBuf != nullptr) {
        outMessage(COD_REQUEST_FAILED, _("Error: %s"), httpResult->pszErrBuf);
        return CPLJSONObject();
    }

    CPLJSONObject result;
    CPLJSONDocument fileJson;
    if(fileJson.LoadMemory(httpResult->pabyData, httpResult->nDataLen)) {
        result = fileJson.GetRoot();
    }
    else {
        outMessage(COD_REQUEST_FAILED, _("Upload file %s failed"), filePath.c_str());
    }
    return result;
}

} // http

} // ngs
