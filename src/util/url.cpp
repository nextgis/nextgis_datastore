/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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

#include "curl/curl.h"
#include "cpl_conv.h"
#include "cpl_http.h"

#include "error.h"
#include "stringutil.h"

namespace ngs {

constexpr unsigned short BUFFER_SIZE = 2048;

static int newProcessFunction(void *p,
                              curl_off_t dltotal, curl_off_t dlnow,
                              curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
{
    Progress* progress = static_cast<Progress*>(p);
    if(progress != nullptr) {
        double dfDone = double(dlnow) / dltotal;
        return progress->onProgress(COD_IN_PROCESS, dfDone, "Uploading ...") ?
                    0 : 1;
    }

    return 0;
}

static int processFunction(void *p, double dltotal, double dlnow,
                                    double ultotal, double ulnow)
{
    return newProcessFunction(p, static_cast<curl_off_t>(dltotal),
                              static_cast<curl_off_t>(dlnow),
                              static_cast<curl_off_t>(ultotal),
                              static_cast<curl_off_t>(ulnow));
}

static size_t bodyWriteFunc(void* buffer, size_t size, size_t nmemb, void* reqInfo)
{
    size_t length = size * nmemb;
    ngsURLRequestResult* result = static_cast<ngsURLRequestResult*>(reqInfo);

    int bytesToWrite = static_cast<int>(length);
    int newSize = result->dataLen + bytesToWrite + 1;
    if(newSize > BUFFER_SIZE) {
        errorMessage("Out of memory allocating %d bytes for HTTP data buffer. The limit is %d",
                     newSize, BUFFER_SIZE);
        return 0;
    }

    memcpy( result->data + result->dataLen, buffer, static_cast<size_t>(bytesToWrite));

    result->dataLen += bytesToWrite;
    result->data[result->dataLen] = 0;

    return length;
}

static size_t headerWriteFunc(void* buffer, size_t size, size_t nmemb, void* reqInfo)
{
    size_t length = size * nmemb;
    ngsURLRequestResult* result = static_cast<ngsURLRequestResult*>(reqInfo);
    char* headerData = static_cast<char*>(CPLCalloc(nmemb + 1, size));
    CPLPrintString(headerData, static_cast<char*>(buffer), static_cast<int>(length));
    char* key = nullptr;
    const char* value = CPLParseNameValue(headerData, &key);
    result->headers = CSLSetNameValue(result->headers, key, value);
    CPLFree(headerData);
    CPLFree(key);
    return length;
}

static std::string getURLOptionValue(const char* key, const Options &options) {
    std::string val = options.asString(key);
    if(val.empty()) {
        val = CPLGetConfigOption(CPLSPrintf("GDAL_HTTP_%s", key), "");
    }
    return val;
}

void urlSetOptions(CURL* http_handle, const Options &options)
{
    if(CPLTestBool(CPLGetConfigOption("CPL_CURL_VERBOSE", "NO"))) {
        curl_easy_setopt(http_handle, CURLOPT_VERBOSE, 1);
    }

    CPLString httpVersion = options.asString("HTTP_VERSION");
    if(compare(httpVersion, "1.0")) {
        curl_easy_setopt(http_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    }

    /* Support control over HTTPAUTH */
    std::string httpAuth = options.asString("HTTPAUTH");
    if(httpAuth.empty()) {
        httpAuth = CPLGetConfigOption("GDAL_HTTP_AUTH", "");
    }
    if(httpAuth.empty())
        /* do nothing */;

    /* CURLOPT_HTTPAUTH is defined in curl 7.11.0 or newer */
#if LIBCURL_VERSION_NUM >= 0x70B00
    else if( compare(httpAuth, "BASIC") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC );
    else if( compare(httpAuth, "NTLM") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_NTLM );
    else if( compare(httpAuth, "ANY") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY );
#ifdef CURLAUTH_GSSNEGOTIATE
    else if( compare(httpAuth, "NEGOTIATE") )
        curl_easy_setopt(http_handle, CURLOPT_HTTPAUTH, CURLAUTH_GSSNEGOTIATE );
#endif
    else {
        errorMessage("Unsupported HTTPAUTH value '%s', ignored.",httpAuth.c_str() );
    }
#else
    else {
        errorMessage("HTTPAUTH option needs curl >= 7.11.0");
    }
#endif

    // Support use of .netrc - default enabled.
    std::string httpNetrc = options.asString("NETRC", "");
    if(httpNetrc.empty()) {
        httpNetrc = CPLGetConfigOption("GDAL_HTTP_NETRC", "YES");
    }

    if(CPLTestBool(httpNetrc.c_str())) {
        curl_easy_setopt(http_handle, CURLOPT_NETRC, 1L);
    }

    // Support setting userid:password.
    std::string userPwd = getURLOptionValue("USERPWD", options);
    if(!userPwd.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_USERPWD, userPwd.c_str());
    }

    // Set Proxy parameters.
    std::string  proxy = getURLOptionValue("PROXY", options);
    if(!proxy.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_PROXY, proxy.c_str());
    }

    std::string proxyUserPwd = getURLOptionValue("PROXYUSERPWD", options);
    if(!proxyUserPwd.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_PROXYUSERPWD, proxyUserPwd.c_str());
    }

    // Support control over PROXYAUTH.
    std::string proxyAuth = options.asString("PROXYAUTH", "");
    if(proxyAuth.empty()) {
        proxyAuth = CPLGetConfigOption( "GDAL_PROXY_AUTH", "");
    }
    if(proxyAuth.empty()) {
        // Do nothing.
    }
    // CURLOPT_PROXYAUTH is defined in curl 7.11.0 or newer.
#if LIBCURL_VERSION_NUM >= 0x70B00
    else if( compare(proxyAuth, "BASIC") )
        curl_easy_setopt(http_handle, CURLOPT_PROXYAUTH, CURLAUTH_BASIC );
    else if( compare(proxyAuth, "NTLM") )
        curl_easy_setopt(http_handle, CURLOPT_PROXYAUTH, CURLAUTH_NTLM );
    else if( compare(proxyAuth, "DIGEST") )
        curl_easy_setopt(http_handle, CURLOPT_PROXYAUTH, CURLAUTH_DIGEST );
    else if( compare(proxyAuth, "ANY") )
        curl_easy_setopt(http_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY );
    else {
        errorMessage("Unsupported PROXYAUTH value '%s', ignored.", proxyAuth.c_str());
    }
#else
    else {
        errorMessage("PROXYAUTH option needs curl >= 7.11.0");
    }
#endif

    // Enable following redirections.  Requires libcurl 7.10.1 at least.
    curl_easy_setopt(http_handle, CURLOPT_FOLLOWLOCATION, 1 );
    curl_easy_setopt(http_handle, CURLOPT_MAXREDIRS, 10 );
    curl_easy_setopt(http_handle, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);

    // Set connect timeout.
    std::string connectTimeout = getURLOptionValue("CONNECTTIMEOUT", options);
    if(!connectTimeout.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_CONNECTTIMEOUT_MS,
                         static_cast<int>(1000 * CPLAtof(connectTimeout.c_str())));
    }

    // Set timeout.
    std::string timeout = getURLOptionValue("TIMEOUT", options);
    if(!timeout.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_TIMEOUT_MS,
                         static_cast<int>(1000 * CPLAtof(timeout.c_str())));
    }

    // Set low speed time and limit.
    std::string lowSpeedTime = getURLOptionValue("LOW_SPEED_TIME", options);
    if(!lowSpeedTime.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_LOW_SPEED_TIME,
                         std::stoi(lowSpeedTime));

        std::string lowSpeedLimit = getURLOptionValue("LOW_SPEED_LIMIT", options);
        if(!lowSpeedLimit.empty()) {
            curl_easy_setopt(http_handle, CURLOPT_LOW_SPEED_LIMIT,
                             std::stoi(lowSpeedLimit) );
        }
        else {
            curl_easy_setopt(http_handle, CURLOPT_LOW_SPEED_LIMIT, 1);
        }
    }

    /* Disable some SSL verification */
    std::string unsafeSSL = getURLOptionValue("UNSAFESSL", options);
    if(CPLTestBool(unsafeSSL.c_str())) {
        curl_easy_setopt(http_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(http_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    // Custom path to SSL certificates.
    std::string CAInfo = options.asString("CAINFO", "");
    if(CAInfo.empty()) {
        // Name of environment variable used by the curl binary
        CAInfo = CPLGetConfigOption("CURL_CA_BUNDLE", "");
    }
    if(CAInfo.empty()) {
        // Name of environment variable used by the curl binary (tested
        // after CURL_CA_BUNDLE
        CAInfo = CPLGetConfigOption("SSL_CERT_FILE", "");
    }
#ifdef WIN32
    if(CAInfo.empty()) {
        CAInfo = CPLFindWin32CurlCaBundleCrt();
    }
#endif
    if(!CAInfo.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_CAINFO, CAInfo.c_str());
    }

    /* Set Referer */
    std::string referer = options.asString("REFERER");
    if(!referer.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_REFERER, referer.c_str());
    }

    /* Set User-Agent */
    std::string userAgent = getURLOptionValue("USERAGENT", options);
    if(!userAgent.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_USERAGENT, userAgent.c_str());
    }

    /* NOSIGNAL should be set to true for timeout to work in multithread
     * environments on Unix, requires libcurl 7.10 or more recent.
     * (this force avoiding the use of signal handlers)
     */
#if LIBCURL_VERSION_NUM >= 0x070A00
    curl_easy_setopt(http_handle, CURLOPT_NOSIGNAL, 1 );
#endif

    std::string cookie = getURLOptionValue("COOKIE", options);
    if(!cookie.empty()) {
        curl_easy_setopt(http_handle, CURLOPT_COOKIE, cookie.c_str());
    }
}

/**
 * @brief uploadFile Uploads file to url
 * @param path Path to file in operating system
 * @param url URL to upload file
 * @param progress Periodically executed progress function with parameters
 * @param options The key=value list of options:
 * - FORM.FILE: name of uplod file path in form
 * - FORM.KEY_0 ... FORM.KEY_N: name of form item
 * - FORM.VALUE_0 ... FORM.VALUE_N: value of the form item
 * - FORM.COUNT: count of form items
 * - CONNECTTIMEOUT=val, where val is in seconds (possibly with decimals). This
 * is the maximum delay for the connection to be established before being aborted
 * - TIMEOUT=val, where val is in seconds. This is the maximum delay for the
 * whole request to complete before being aborted
 * - LOW_SPEED_TIME=val, where val is in seconds. This is the maximum time where
 * the transfer speed should be below the LOW_SPEED_LIMIT (if not specified 1b/s),
 * before the transfer to be considered too slow and aborted
 * - LOW_SPEED_LIMIT=val, where val is in bytes/second. See LOW_SPEED_TIME. Has
 * only effect if LOW_SPEED_TIME is specified too
 * - HEADERS=val, where val is an extra header to use when getting a web page.
 * For example "Accept: application/x-ogcwkt"
 * - HTTPAUTH=[BASIC/NTLM/GSSNEGOTIATE/ANY] to specify an authentication scheme
 * to use
 * - USERPWD=userid:password to specify a user and password for authentication
 * - PROXY=val, to make requests go through a proxy server, where val is of the
 * form proxy.server.com:port_number
 * - PROXYUSERPWD=val, where val is of the form username:password
 * - PROXYAUTH=[BASIC/NTLM/DIGEST/ANY] to specify an proxy authentication scheme
 * to use
 * - NETRC=[YES/NO] to enable or disable use of $HOME/.netrc, default YES
 * - COOKIE=val, where val is formatted as COOKIE1=VALUE1; COOKIE2=VALUE2; ...
 * - MAX_FILE_SIZE=val, where val is a number of bytes
 * - CAINFO=/path/to/bundle.crt. This is path to Certificate Authority (CA)
 * bundle file. By default, it will be looked in a system location. If the
 * CAINFO options is not defined, GDAL will also look if the CURL_CA_BUNDLE
 * environment variable is defined to use it as the CAINFO value, and as a
 * fallback to the SSL_CERT_FILE environment variable
 * @return ngsURLRequestResult structure. The result must be freed by caller via
 * delete
 */

ngsURLRequestResult* uploadFile(const std::string &path, const std::string &url,
                                const Progress &progress, const Options &options)
{
    CURL* http_handle = curl_easy_init();
    ngsURLRequestResult* out = new ngsURLRequestResult;
    out->dataLen = 0;
    out->data = nullptr;
    out->headers = nullptr;

    const char* authHeader = CPLHTTPAuthHeader(url.c_str());
    if(compare(authHeader, "expired")) {
        out->status = 401;
        return out;
    }

    out->data = new unsigned char[BUFFER_SIZE];
    curl_easy_setopt(http_handle, CURLOPT_URL, url.c_str());

    urlSetOptions(http_handle, options);

    struct curl_slist* headers = nullptr;
    if(!EQUAL(authHeader, "")) {
        headers = curl_slist_append(headers, authHeader);
    }

    // Set Headers.
    std::string addHeaders = options.asString("HEADERS", "");
    if(!addHeaders.empty()) {
        CPLDebug("ngstore", "These HTTP headers were set: %s", addHeaders.c_str());
        char** papszTokensHeaders = CSLTokenizeString2(addHeaders.c_str(),
                                                       "\r\n", 0);
        for(int i = 0; papszTokensHeaders[i] != nullptr; ++i) {
            headers = curl_slist_append(headers, papszTokensHeaders[i]);
        }
        CSLDestroy(papszTokensHeaders);
    }

    if(headers != nullptr) {
         curl_easy_setopt(http_handle, CURLOPT_HTTPHEADER, headers);
    }

    // Fill form
    struct curl_httppost* formpost = nullptr;
    struct curl_httppost* lastptr = nullptr;

    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, options.asString("FORM.FILE").c_str(),
                 CURLFORM_FILE, path.c_str(),
                 CURLFORM_END);
    CPLDebug("ngstore", "Send file: %s, COPYNAME: %s", path.c_str(),
             options.asString("FORM.FILE").c_str());

    int parametersCount = options.asInt("FORM.COUNT", 0);
    for(int i = 0; i < parametersCount; ++i) {
        CPLString name = CPLSPrintf("FORM.KEY_%d", i);
        CPLString value = CPLSPrintf("FORM.VALUE_%d", i);
        curl_formadd(&formpost, &lastptr,
                     CURLFORM_COPYNAME, options.asString(name).c_str(),
                     CURLFORM_COPYCONTENTS, options.asString(value).c_str(),
                     CURLFORM_END);
        CPLDebug("ngstore", "COPYNAME: %s, COPYCONTENTS: %s", options.asString(name).c_str(),
                 options.asString(value).c_str());
    }

    curl_easy_setopt(http_handle, CURLOPT_HTTPPOST, formpost);

    // Capture response headers.
    curl_easy_setopt(http_handle, CURLOPT_HEADERDATA, out);
    curl_easy_setopt(http_handle, CURLOPT_HEADERFUNCTION, headerWriteFunc);

    curl_easy_setopt(http_handle, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(http_handle, CURLOPT_WRITEFUNCTION, bodyWriteFunc);

    char curlErrBuf[CURL_ERROR_SIZE + 1] = {};
    curlErrBuf[0] = '\0';
    curl_easy_setopt(http_handle, CURLOPT_ERRORBUFFER, curlErrBuf );

    static bool bHasCheckVersion = false;
    static bool bSupportGZip = false;
    if(!bHasCheckVersion) {
        bSupportGZip = strstr(curl_version(), "zlib/") != nullptr;
        bHasCheckVersion = true;
    }
    bool bGZipRequested = false;
    if(bSupportGZip && CPLTestBool(CPLGetConfigOption("CPL_CURL_GZIP", "YES"))) {
        bGZipRequested = true;
        curl_easy_setopt(http_handle, CURLOPT_ENCODING, "gzip");
    }

    curl_easy_setopt(http_handle, CURLOPT_PROGRESSFUNCTION, processFunction);
    curl_easy_setopt(http_handle, CURLOPT_PROGRESSDATA, &progress);

#if LIBCURL_VERSION_NUM >= 0x072000
    curl_easy_setopt(http_handle, CURLOPT_XFERINFOFUNCTION, newProcessFunction);
    curl_easy_setopt(http_handle, CURLOPT_XFERINFODATA, &progress);
#endif
    curl_easy_setopt(http_handle, CURLOPT_NOPROGRESS, 0L);


    CURLcode resultCode = curl_easy_perform(http_handle);
    if(resultCode == CURLE_COULDNT_RESOLVE_HOST) {
        // Second try
        resultCode = curl_easy_perform(http_handle);
    }

    out->status = 543;
    curl_easy_getinfo(http_handle, CURLINFO_RESPONSE_CODE, &out->status);

    if(strlen(curlErrBuf) > 0) {
        errorMessage(_("CURL error: %s"), curlErrBuf);
    }

    curl_easy_cleanup(http_handle);

    curl_formfree(formpost);
    curl_slist_free_all(headers);

    return out;
}

}

/*

PERFORMRESULT wxGISCurlRefData::UploadFiles(const wxString & sURL, const wxArrayString& asFilePaths, ITrackCancel* const pTrackCancel)
{
    wxCriticalSectionLocker lock(m_CritSect);
    PERFORMRESULT result;
        result.IsValid = false;
        result.iSize = 0;
        result.nHTTPCode = 0;
        struct curl_httppost *formpost=NULL;
        struct curl_httppost *lastptr=NULL;

        for ( size_t i = 0; i < asFilePaths.GetCount(); ++i )
        {
                CPLString szFilePath(asFilePaths[i].ToUTF8());

            //CPLString szFilePath = CPLString(Transliterate(asFilePaths[i]).ToUTF8());

                curl_formadd(&formpost, &lastptr,
                        CURLFORM_COPYNAME, "files[]",
            CURLFORM_FILE, (const char*)asFilePaths[i].mb_str(), //sys encoding
            CURLFORM_FILENAME, CPLGetFilename(szFilePath),       //utf encoding
                        CURLFORM_END);
                }

        curl_easy_setopt(m_pCurl, CURLOPT_URL, (const char*)sURL.ToUTF8());
    curl_easy_setopt(m_pCurl, CURLOPT_HTTPPOST, formpost);

#if LIBCURL_VERSION_NUM >= 0x072000
    struct ProgressStruct prog = { true, pTrackCancel };
    if (pTrackCancel)
    {
        curl_easy_setopt(m_pCurl, CURLOPT_XFERINFOFUNCTION, xferinfo);
        curl_easy_setopt(m_pCurl, CURLOPT_XFERINFODATA, &prog);
        curl_easy_setopt(m_pCurl, CURLOPT_NOPROGRESS, 0L);
    }
    else
    {
        curl_easy_setopt(m_pCurl, CURLOPT_NOPROGRESS, 1L);
    }
#endif

    headstruct.size = 0;
    headstruct.pFile = NULL;
    bodystruct.size = 0;
    bodystruct.pFile = NULL;

        res = curl_easy_perform(m_pCurl);

        //second try
        if(res == CURLE_COULDNT_RESOLVE_HOST)
                res = curl_easy_perform(m_pCurl);
        //

        curl_easy_getinfo (m_pCurl, CURLINFO_RESPONSE_CODE, &result.nHTTPCode);

        if(res == CURLE_OK)
        {
                result.sHead = wxString((const char*)headstruct.memory, headstruct.size);//wxConvLocal,
        //charset
        int posb = result.sHead.Find(wxT("charset="));
        wxString soSet;//(wxT("default"));
        if( posb != wxNOT_FOUND)
        {
            soSet = result.sHead.Mid(posb + 8, 50);
            int pose = soSet.Find(wxT("\r\n"));
            soSet = soSet.Left(pose);
        }

        if( soSet.IsSameAs(wxT("utf-8"), false) || soSet.IsSameAs(wxT("utf8"), false) )
        {
            result.sBody = wxString((const char*)bodystruct.memory, wxConvUTF8, bodystruct.size);
        }
        else if( soSet.IsSameAs(wxT("utf-16"), false) || soSet.IsSameAs(wxT("utf16"), false) )
        {
            result.sBody = wxString((const char*)bodystruct.memory, wxConvUTF8, bodystruct.size);
        }
        else if( soSet.IsSameAs(wxT("utf-32"), false) || soSet.IsSameAs(wxT("utf32"), false) )
        {
            result.sBody = wxString((const char*)bodystruct.memory, wxConvUTF8, bodystruct.size);
        }
        else if( soSet.IsSameAs(wxT("utf-7"), false) || soSet.IsSameAs(wxT("utf7"), false) )
        {
            result.sBody = wxString((const char*)bodystruct.memory, wxConvUTF8, bodystruct.size);
        }
        else
        {
            //wxCSConv
            wxCSConv conv(soSet);

            if(conv.IsOk())
                result.sBody = wxString((const char*)bodystruct.memory, conv, bodystruct.size);
            else
                result.sBody = wxString((const char*)bodystruct.memory, *wxConvCurrent, bodystruct.size);//wxConvLocal,
        }

                result.iSize = headstruct.size + bodystruct.size;
                result.IsValid = true;
        }
        return result;
}
*/
