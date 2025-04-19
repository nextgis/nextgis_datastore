/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2025 NextGIS, <info@nextgis.com>
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

#include <curl/curl.h>
#include "cpl_multiproc.h"

#include "authstore.h"
#include "catalog/file.h"
#include "error.h"
#include "stringutil.h"
#include "settings.h"

#define unchecked_curl_easy_setopt(handle, opt, param) \
    CPL_IGNORE_RET_VAL(curl_easy_setopt(handle, opt, param))

typedef struct
{
    GDALProgressFunc pfnProgress;
    void *pProgressArg;
} CurlProcessData, *CurlProcessDataL;

namespace ngs
{

    namespace http
    {
        static CPLMutex *hSessionMapMutex = nullptr;
        static std::map<CPLString, CURL *> *poSessionMap = nullptr;
        static bool bSupportGZip = false;

        struct callbackUserDataStruct
        {
            CPLString osURL{};
            CSLConstList papszOptions = nullptr;
            GDALProgressFunc pfnProgress = nullptr;
            void *pProgressArg = nullptr;
            CPLHTTPFetchWriteFunc pfnWrite = nullptr;
            void *pWriteArg = nullptr;
        };

        static void httpFreeFunction(void *arg)
        {
            VSIFCloseL(static_cast<VSILFILE *>(arg));
        }

        //------------------------------------------------------------------------------
        // httpFetchCleanup
        //------------------------------------------------------------------------------
        static void httpFetchCleanup(CURL *http_handle, struct curl_slist *headers,
                                     const char *pszPersistent, CSLConstList papszOptions)
        {
            if (CSLFetchNameValue(papszOptions, "POSTFIELDS"))
                unchecked_curl_easy_setopt(http_handle, CURLOPT_POST, 0);
            unchecked_curl_easy_setopt(http_handle, CURLOPT_HTTPHEADER, nullptr);

            if (!pszPersistent)
                curl_easy_cleanup(http_handle);

            curl_slist_free_all(headers);
        }

        static size_t httpReadFunction(char *buffer, size_t size, size_t nitems,
                                       void *arg)
        {
            return VSIFReadL(buffer, size, nitems, static_cast<VSILFILE *>(arg));
        }

        static int httpSeekFunction(void *arg, curl_off_t offset, int origin)
        {
            if (VSIFSeekL(static_cast<VSILFILE *>(arg), offset, origin) == 0)
            {
                return CURL_SEEKFUNC_OK;
            }
            else
            {
                return CURL_SEEKFUNC_FAIL;
            }
        }

        //------------------------------------------------------------------------------
        // PostFields
        //------------------------------------------------------------------------------

        class PostFields
        {
        public:
            PostFields() = default;
            PostFields &operator=(const PostFields &) = delete;
            PostFields(const PostFields &) = delete;

            CPLErr Fill(CURL *http_handle, CSLConstList papszOptions)
            {
                // Fill POST form if present
                const char *pszFormFilePath =
                    CSLFetchNameValue(papszOptions, "FORM_FILE_PATH");
                const char *pszParametersCount =
                    CSLFetchNameValue(papszOptions, "FORM_ITEM_COUNT");

                if (pszFormFilePath != nullptr || pszParametersCount != nullptr)
                {
                    mime = curl_mime_init(http_handle);
                    curl_mimepart *mimepart = curl_mime_addpart(mime);
                    if (pszFormFilePath != nullptr)
                    {
                        const char *pszFormFileName =
                            CSLFetchNameValue(papszOptions, "FORM_FILE_NAME");
                        const char *pszFilename = CPLGetFilename(pszFormFilePath);
                        if (pszFormFileName == nullptr)
                        {
                            pszFormFileName = pszFilename;
                        }

                        VSIStatBufL sStat;
                        if (VSIStatL(pszFormFilePath, &sStat) == 0)
                        {
                            VSILFILE *mime_fp = VSIFOpenL(pszFormFilePath, "rb");
                            if (mime_fp != nullptr)
                            {
                                curl_mime_name(mimepart, pszFormFileName);
                                CPL_IGNORE_RET_VAL(
                                    curl_mime_filename(mimepart, pszFilename));
                                curl_mime_data_cb(
                                    mimepart, sStat.st_size, httpReadFunction,
                                    httpSeekFunction, httpFreeFunction, mime_fp);
                            }
                            else
                            {
                                osErrMsg = CPLSPrintf("Failed to open file %s",
                                                      pszFormFilePath);
                                return CE_Failure;
                            }

                            CPLDebug("HTTP", "Send file: %s, COPYNAME: %s",
                                     pszFormFilePath, pszFormFileName);
                        }
                        else
                        {
                            osErrMsg =
                                CPLSPrintf("File '%s' not found", pszFormFilePath);
                            return CE_Failure;
                        }
                    }

                    int nParametersCount = 0;
                    if (pszParametersCount != nullptr)
                    {
                        nParametersCount = atoi(pszParametersCount);
                    }

                    for (int i = 0; i < nParametersCount; ++i)
                    {
                        const char *pszKey = CSLFetchNameValue(
                            papszOptions, CPLSPrintf("FORM_KEY_%d", i));
                        const char *pszValue = CSLFetchNameValue(
                            papszOptions, CPLSPrintf("FORM_VALUE_%d", i));

                        if (nullptr == pszKey)
                        {
                            osErrMsg = CPLSPrintf("Key #%d is not exists. Maybe wrong "
                                                  "count of form items",
                                                  i);
                            return CE_Failure;
                        }

                        if (nullptr == pszValue)
                        {
                            osErrMsg = CPLSPrintf("Value #%d is not exists. Maybe "
                                                  "wrong count of form items",
                                                  i);
                            return CE_Failure;
                        }

                        mimepart = curl_mime_addpart(mime);
                        curl_mime_name(mimepart, pszKey);
                        CPL_IGNORE_RET_VAL(
                            curl_mime_data(mimepart, pszValue, CURL_ZERO_TERMINATED));

                        CPLDebug("HTTP", "COPYNAME: %s, COPYCONTENTS: %s", pszKey,
                                 pszValue);
                    }

                    unchecked_curl_easy_setopt(http_handle, CURLOPT_MIMEPOST, mime);
                }
                return CE_None;
            }

            ~PostFields()
            {
                if (mime != nullptr)
                {
                    curl_mime_free(mime);
                }
            }

            std::string GetErrorMessage() const
            {
                return osErrMsg;
            }

        private:
            curl_mime *mime = nullptr;
            std::string osErrMsg{};
        };

        //------------------------------------------------------------------------------
        // HTTPResultWithLimit
        //------------------------------------------------------------------------------

        class HTTPResultWithLimit
        {
        public:
            CPLHTTPResult *psResult = nullptr;
            int nMaxFileSize = 0;
        };

        static size_t headerWriteFct(void *buffer, size_t size, size_t nmemb,
                                     void *reqInfo)
        {
            CPLHTTPResult *psResult = static_cast<CPLHTTPResult *>(reqInfo);
            // Copy the buffer to a char* and initialize with zeros (zero
            // terminate as well).
            size_t nBytes = size * nmemb;
            char *pszHdr = static_cast<char *>(CPLCalloc(1, nBytes + 1));
            memcpy(pszHdr, buffer, nBytes);
            size_t nIdx = nBytes - 1;
            // Remove end of line characters
            while (nIdx > 0 && (pszHdr[nIdx] == '\r' || pszHdr[nIdx] == '\n'))
            {
                pszHdr[nIdx] = 0;
                nIdx--;
            }
            char *pszKey = nullptr;
            const char *pszValue = CPLParseNameValue(pszHdr, &pszKey);
            if (pszKey && pszValue)
            {
                psResult->papszHeaders =
                    CSLAddNameValue(psResult->papszHeaders, pszKey, pszValue);
            }
            CPLFree(pszHdr);
            CPLFree(pszKey);
            return nmemb;
        }

        static int newProcessFunction(void *p, curl_off_t dltotal, curl_off_t dlnow,
                                      curl_off_t ultotal, curl_off_t ulnow)
        {
            CurlProcessDataL pData = static_cast<CurlProcessDataL>(p);
            if (nullptr != pData && pData->pfnProgress)
            {
                if (dltotal > 0)
                {
                    const double dfDone = double(dlnow) / dltotal;
                    return pData->pfnProgress(dfDone, _("Downloading ..."),
                                              pData->pProgressArg) == TRUE
                               ? 0
                               : 1;
                }
                else if (ultotal > 0)
                {
                    const double dfDone = double(ulnow) / ultotal;
                    return pData->pfnProgress(dfDone, _("Uploading ..."),
                                              pData->pProgressArg) == TRUE
                               ? 0
                               : 1;
                }
            }
            return 0;
        }

        static double httpGetNewRetryDelay(int response_code,
                                           double dfOldDelay,
                                           const char *pszErrBuf,
                                           const char *pszCurlError,
                                           const char *pszRetriableCodes)
        {
            bool bRetry = false;
            if (pszRetriableCodes && pszRetriableCodes[0])
            {
                bRetry = EQUAL(pszRetriableCodes, "ALL") ||
                         strstr(pszRetriableCodes, CPLSPrintf("%d", response_code));
            }
            else if (response_code == 429 || response_code == 500 ||
                     (response_code >= 502 && response_code <= 504) ||
                     // S3 sends some client timeout errors as 400 Client Error
                     (response_code == 400 && pszErrBuf &&
                      strstr(pszErrBuf, "RequestTimeout")) ||
                     (pszCurlError &&
                      (strstr(pszCurlError, "Connection timed out") ||
                       strstr(pszCurlError, "Operation timed out") ||
                       strstr(pszCurlError, "Connection reset by peer") ||
                       strstr(pszCurlError, "Connection was reset"))))
            {
                bRetry = true;
            }
            if (bRetry)
            {
                return dfOldDelay * (2 + rand() * 0.5 / RAND_MAX);
            }
            else
            {
                return 0;
            }
        }

        static size_t writeFct(void *buffer, size_t size, size_t nmemb, void *reqInfo)

        {
            HTTPResultWithLimit *psResultWithLimit =
                static_cast<HTTPResultWithLimit *>(reqInfo);
            CPLHTTPResult *psResult = psResultWithLimit->psResult;

            int nBytesToWrite = static_cast<int>(nmemb) * static_cast<int>(size);
            int nNewSize = psResult->nDataLen + nBytesToWrite + 1;
            if (nNewSize > psResult->nDataAlloc)
            {
                psResult->nDataAlloc = static_cast<int>(nNewSize * 1.25 + 100);
                GByte *pabyNewData = static_cast<GByte *>(
                    VSIRealloc(psResult->pabyData, psResult->nDataAlloc));
                if (pabyNewData == nullptr)
                {
                    VSIFree(psResult->pabyData);
                    psResult->pabyData = nullptr;
                    psResult->pszErrBuf = CPLStrdup(CPLString().Printf(
                        "Out of memory allocating %d bytes for HTTP data buffer.",
                        psResult->nDataAlloc));
                    psResult->nDataAlloc = psResult->nDataLen = 0;

                    return 0;
                }
                psResult->pabyData = pabyNewData;
            }

            memcpy(psResult->pabyData + psResult->nDataLen, buffer, nBytesToWrite);

            psResult->nDataLen += nBytesToWrite;
            psResult->pabyData[psResult->nDataLen] = 0;

            if (psResultWithLimit->nMaxFileSize > 0 &&
                psResult->nDataLen > psResultWithLimit->nMaxFileSize)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Maximum file size reached");
                return 0;
            }

            return nmemb;
        }

        static CPLHTTPResult *httpFetchCallback(const char *pszURL, CSLConstList papszOptions,
                                                GDALProgressFunc pfnProgress, void *pProgressArg,
                                                CPLHTTPFetchWriteFunc pfnWrite, void *pWriteArg)
        {
            CURL *http_handle = nullptr;

            const char *pszPersistent = CSLFetchNameValue(papszOptions, "PERSISTENT");
            const char *pszClosePersistent =
                CSLFetchNameValue(papszOptions, "CLOSE_PERSISTENT");
            if (pszPersistent)
            {
                CPLString osSessionName = pszPersistent;
                CPLMutexHolder oHolder(&hSessionMapMutex);

                if (poSessionMap == nullptr)
                    poSessionMap = new std::map<CPLString, CURL *>;
                if (poSessionMap->count(osSessionName) == 0)
                {
                    (*poSessionMap)[osSessionName] = curl_easy_init();
                    CPLDebug("HTTP", "Establish persistent session named '%s'.",
                             osSessionName.c_str());
                }

                http_handle = (*poSessionMap)[osSessionName];
            }
            else if (pszClosePersistent)
            {
                CPLString osSessionName = pszClosePersistent;
                CPLMutexHolder oHolder(&hSessionMapMutex);

                if (poSessionMap)
                {
                    std::map<CPLString, CURL *>::iterator oIter =
                        poSessionMap->find(osSessionName);
                    if (oIter != poSessionMap->end())
                    {
                        curl_easy_cleanup(oIter->second);
                        poSessionMap->erase(oIter);
                        if (poSessionMap->empty())
                        {
                            delete poSessionMap;
                            poSessionMap = nullptr;
                        }
                        CPLDebug("HTTP", "Ended persistent session named '%s'.",
                                 osSessionName.c_str());
                    }
                    else
                    {
                        CPLDebug("HTTP",
                                 "Could not find persistent session named '%s'.",
                                 osSessionName.c_str());
                    }
                }

                return nullptr;
            }
            else
                http_handle = curl_easy_init();

            char szCurlErrBuf[CURL_ERROR_SIZE + 1] = {};

            CPLHTTPResult *psResult =
                static_cast<CPLHTTPResult *>(CPLCalloc(1, sizeof(CPLHTTPResult)));

            struct curl_slist *headers = reinterpret_cast<struct curl_slist *>(
                CPLHTTPSetOptions(http_handle, pszURL, papszOptions));
            if (headers != nullptr)
                unchecked_curl_easy_setopt(http_handle, CURLOPT_HTTPHEADER, headers);

            // Are we making a head request.
            const char *pszNoBody = nullptr;
            if ((pszNoBody = CSLFetchNameValue(papszOptions, "NO_BODY")) != nullptr)
            {
                if (CPLTestBool(pszNoBody))
                {
                    CPLDebug("HTTP", "HEAD Request: %s", pszURL);
                    unchecked_curl_easy_setopt(http_handle, CURLOPT_NOBODY, 1L);
                }
            }

            // Capture response headers.
            unchecked_curl_easy_setopt(http_handle, CURLOPT_HEADERDATA, psResult);
            unchecked_curl_easy_setopt(http_handle, CURLOPT_HEADERFUNCTION,
                                       headerWriteFct);

            HTTPResultWithLimit sResultWithLimit;
            if (nullptr == pfnWrite)
            {
                pfnWrite = writeFct;

                sResultWithLimit.psResult = psResult;
                sResultWithLimit.nMaxFileSize = 0;
                const char *pszMaxFileSize =
                    CSLFetchNameValue(papszOptions, "MAX_FILE_SIZE");
                if (pszMaxFileSize != nullptr)
                {
                    sResultWithLimit.nMaxFileSize = atoi(pszMaxFileSize);
                    // Only useful if size is returned by server before actual download.
                    unchecked_curl_easy_setopt(http_handle, CURLOPT_MAXFILESIZE,
                                               sResultWithLimit.nMaxFileSize);
                }
                pWriteArg = &sResultWithLimit;
            }

            unchecked_curl_easy_setopt(http_handle, CURLOPT_WRITEDATA, pWriteArg);
            unchecked_curl_easy_setopt(http_handle, CURLOPT_WRITEFUNCTION, pfnWrite);

            CurlProcessData stProcessData = {pfnProgress, pProgressArg};
            if (nullptr != pfnProgress)
            {
                unchecked_curl_easy_setopt(http_handle, CURLOPT_XFERINFOFUNCTION,
                                           newProcessFunction);
                unchecked_curl_easy_setopt(http_handle, CURLOPT_XFERINFODATA,
                                           &stProcessData);
                unchecked_curl_easy_setopt(http_handle, CURLOPT_NOPROGRESS, 0L);
            }

            szCurlErrBuf[0] = '\0';

            unchecked_curl_easy_setopt(http_handle, CURLOPT_ERRORBUFFER, szCurlErrBuf);

            bool bGZipRequested = false;
            if (bSupportGZip && CPLTestBool(CPLGetConfigOption("CPL_CURL_GZIP", "YES")))
            {
                bGZipRequested = true;
                unchecked_curl_easy_setopt(http_handle, CURLOPT_ENCODING, "gzip");
            }

            PostFields oPostFields;
            if (oPostFields.Fill(http_handle, papszOptions) != CE_None)
            {
                psResult->nStatus = 34; // CURLE_HTTP_POST_ERROR
                psResult->pszErrBuf = CPLStrdup(oPostFields.GetErrorMessage().c_str());
                CPLError(CE_Failure, CPLE_AppDefined, "%s", psResult->pszErrBuf);
                httpFetchCleanup(http_handle, headers, pszPersistent, papszOptions);
                return psResult;
            }

            const char *pszRetryDelay = CSLFetchNameValue(papszOptions, "RETRY_DELAY");
            if (pszRetryDelay == nullptr)
                pszRetryDelay = CPLGetConfigOption(
                    "GDAL_HTTP_RETRY_DELAY", CPLSPrintf("%f", CPL_HTTP_RETRY_DELAY));
            const char *pszMaxRetries = CSLFetchNameValue(papszOptions, "MAX_RETRY");
            if (pszMaxRetries == nullptr)
                pszMaxRetries = CPLGetConfigOption(
                    "GDAL_HTTP_MAX_RETRY", CPLSPrintf("%d", CPL_HTTP_MAX_RETRY));
            // coverity[tainted_data]
            double dfRetryDelaySecs = CPLAtof(pszRetryDelay);
            int nMaxRetries = atoi(pszMaxRetries);
            const char *pszRetryCodes = CSLFetchNameValue(papszOptions, "RETRY_CODES");
            if (!pszRetryCodes)
                pszRetryCodes = CPLGetConfigOption("GDAL_HTTP_RETRY_CODES", nullptr);
            int nRetryCount = 0;

            while (true)
            {
                void *old_handler = CPLHTTPIgnoreSigPipe();
                psResult->nStatus = static_cast<int>(curl_easy_perform(http_handle));
                CPLHTTPRestoreSigPipeHandler(old_handler);

                psResult->pszContentType = nullptr;
                curl_easy_getinfo(http_handle, CURLINFO_CONTENT_TYPE,
                                  &(psResult->pszContentType));
                if (psResult->pszContentType != nullptr)
                    psResult->pszContentType = CPLStrdup(psResult->pszContentType);

                long response_code = 0;
                curl_easy_getinfo(http_handle, CURLINFO_RESPONSE_CODE, &response_code);
                if (response_code != 200)
                {
                    const double dfNewRetryDelay = httpGetNewRetryDelay(
                        static_cast<int>(response_code), dfRetryDelaySecs,
                        reinterpret_cast<const char *>(psResult->pabyData),
                        szCurlErrBuf, pszRetryCodes);
                    if (dfNewRetryDelay > 0 && nRetryCount < nMaxRetries)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "HTTP error code: %d - %s. "
                                 "Retrying again in %.1f secs",
                                 static_cast<int>(response_code), pszURL,
                                 dfRetryDelaySecs);
                        CPLSleep(dfRetryDelaySecs);
                        dfRetryDelaySecs = dfNewRetryDelay;
                        nRetryCount++;

                        CPLFree(psResult->pszContentType);
                        psResult->pszContentType = nullptr;
                        CSLDestroy(psResult->papszHeaders);
                        psResult->papszHeaders = nullptr;
                        CPLFree(psResult->pabyData);
                        psResult->pabyData = nullptr;
                        psResult->nDataLen = 0;
                        psResult->nDataAlloc = 0;

                        continue;
                    }
                }

                if (strlen(szCurlErrBuf) > 0)
                {
                    bool bSkipError = false;
                    const char *pszContentLength =
                        CSLFetchNameValue(psResult->papszHeaders, "Content-Length");

                    if (bGZipRequested &&
                        strstr(szCurlErrBuf, "transfer closed with") &&
                        strstr(szCurlErrBuf, "bytes remaining to read"))
                    {
                        if (pszContentLength && psResult->nDataLen != 0 &&
                            atoi(pszContentLength) == psResult->nDataLen)
                        {
                            const char *pszCurlGZIPOption =
                                CPLGetConfigOption("CPL_CURL_GZIP", nullptr);
                            if (pszCurlGZIPOption == nullptr)
                            {
                                CPLSetConfigOption("CPL_CURL_GZIP", "NO");
                                CPLDebug("HTTP",
                                         "Disabling CPL_CURL_GZIP, "
                                         "because %s doesn't support it properly",
                                         pszURL);
                            }
                            psResult->nStatus = 0;
                            bSkipError = true;
                        }
                    }

                    else if (pszContentLength == nullptr &&
                             (strstr(szCurlErrBuf,
                                     "GnuTLS recv error (-110): The TLS connection was "
                                     "non-properly terminated") != nullptr ||
                              strstr(szCurlErrBuf,
                                     "SSL_read: error:0A000126:SSL "
                                     "routines::unexpected eof while reading") !=
                                  nullptr))
                    {
                        psResult->nStatus = 0;
                        bSkipError = true;
                    }
                    else if (CPLTestBool(
                                 CPLGetConfigOption("CPL_CURL_IGNORE_ERROR", "NO")))
                    {
                        psResult->nStatus = 0;
                        bSkipError = true;
                    }

                    if (!bSkipError)
                    {
                        psResult->pszErrBuf = CPLStrdup(szCurlErrBuf);
                        if (psResult->nDataLen > 0)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "%s. You may set the CPL_CURL_IGNORE_ERROR "
                                     "configuration option to YES to try to ignore it.",
                                     szCurlErrBuf);
                        }
                        else
                        {
                            CPLError(CE_Failure, CPLE_AppDefined, "%s", szCurlErrBuf);
                        }
                    }
                }
                else
                {
                    if (response_code >= 400 && response_code < 600)
                    {
                        psResult->pszErrBuf = CPLStrdup(CPLSPrintf(
                            "HTTP error code : %d", static_cast<int>(response_code)));
                        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                                 psResult->pszErrBuf);
                    }
                }
                break;
            }

            httpFetchCleanup(http_handle, headers, pszPersistent, papszOptions);

            return psResult;
        }

        auto httpCbk = [](const char *pszURL, CSLConstList papszOptions,
                          GDALProgressFunc pfnProgress, void *pProgressArg,
                          CPLHTTPFetchWriteFunc pfnWrite, void *pWriteArg,
                          void *pUserData)
        {
            callbackUserDataStruct *pCbkUserData =
                static_cast<callbackUserDataStruct *>(pUserData);

            Options opt(CSLDuplicate(papszOptions));
            auto requestOptions = addAuthHeaders(pszURL, opt);

            return httpFetchCallback(pszURL, requestOptions.asStringList(),
                                     pfnProgress, pProgressArg, pfnWrite, pWriteArg);
        };

        bool pushFetchCallback()
        {
            callbackUserDataStruct userData;
            return CPLHTTPPushFetchCallback(httpCbk, &userData) == 1;
        }

        bool popFetchCallback()
        {
            return CPLHTTPPopFetchCallback() == 1;
        }

        //------------------------------------------------------------------------------
        // HTTPResultPtr
        //------------------------------------------------------------------------------

        HTTPResultPtr::HTTPResultPtr(CPLHTTPResult *result) : shared_ptr(result, CPLHTTPDestroyResult)
        {
        }

        HTTPResultPtr::HTTPResultPtr() : shared_ptr(nullptr, CPLHTTPDestroyResult)
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
        // ngsURLRequestResultPtr
        //------------------------------------------------------------------------------

        ngsURLRequestResultPtr::ngsURLRequestResultPtr(
            ngsURLRequestResult *result) : shared_ptr(result, ngsURLRequestResultFree)
        {
        }

        //------------------------------------------------------------------------------
        // httpFetch
        //------------------------------------------------------------------------------

        ngsURLRequestResult *httpFetch(const std::string &url, const Progress &progress,
                                       const Options &options)
        {
            resetError();
            auto requestOptions = addAuthHeaders(url, options);

            Progress progressIn(progress);
            HTTPResultPtr result = httpFetchCallback(url.c_str(), requestOptions.asStringList(),
                                                     ngsGDALProgress, &progressIn, nullptr,
                                                     nullptr);
            if (nullptr == result)
            {
                putMessage(COD_REQUEST_FAILED, _("Unexpected error"));
                return nullptr;
            }
            if (result->nStatus != 0 || result->pszErrBuf != nullptr)
            {
                std::string errorMessageStr(result->pszErrBuf);
                CPLJSONDocument resultDoc;
                if (resultDoc.LoadMemory(result->pabyData, result->nDataLen))
                {
                    CPLJSONObject root = resultDoc.GetRoot();
                    if (root.IsValid())
                    {
                        errorMessageStr = root.GetString("message");
                        if (errorMessageStr.empty())
                        {
                            errorMessageStr = root.GetString("error");
                            if (errorMessageStr.empty())
                            {
                                errorMessageStr = std::string(result->pszErrBuf);
                            }
                        }
                    }
                }

                putMessage(COD_REQUEST_FAILED, errorMessageStr.c_str());
                ngsURLRequestResult *out = new ngsURLRequestResult;
                out->status = result->nStatus;
                out->headers = nullptr;
                out->dataLen = 0;
                out->data = nullptr;
                return out;
            }

            ngsURLRequestResult *out = new ngsURLRequestResult;
            out->status = result->nStatus;
            out->headers = result->papszHeaders;
            out->dataLen = result->nDataLen;
            out->data = result->pabyData;

            // Transfer own to out, don't delete with result
            result->papszHeaders = nullptr;
            result->pabyData = nullptr;

            return out;
        }

        CPLJSONObject jsonFetch(const std::string &url, const Progress &progress,
                                const Options &options)
        {
            CPLJSONDocument doc;
            auto requestOptions = addAuthHeaders(url, options);
            Progress progressIn(progress);
            if (doc.LoadUrl(url, requestOptions.asStringList(), ngsGDALProgress, &progressIn))
            {
                return doc.GetRoot();
            }
            return CPLJSONObject();
        }

        bool getFile(const std::string &url, const std::string &path,
                     const Progress &progress, const Options &options)
        {
            resetError();
            VSILFILE *const fp = VSIFOpenL(path.c_str(), "wb");
            if (fp == nullptr)
            {
                return errorMessage(_("Create file %s failed"), path.c_str());
            }

            auto requestOptions = addAuthHeaders(url, options);
            Progress progressIn(progress);
            HTTPResultPtr result = CPLHTTPFetchEx(url.c_str(), requestOptions.asStringList(),
                                                  ngsGDALProgress, &progressIn,
                                                  writeFct, fp);

            bool ret = VSIFCloseL(fp) == 0;
            if (nullptr == result)
            {
                putMessage(COD_REQUEST_FAILED, _("Unexpected error"));
                return false;
            }
            if (result->nStatus != 0 || result->pszErrBuf != nullptr)
            {
                std::string errorMessageStr(result->pszErrBuf);
                CPLJSONDocument resultDoc;
                if (resultDoc.LoadMemory(result->pabyData, result->nDataLen))
                {
                    CPLJSONObject root = resultDoc.GetRoot();
                    if (root.IsValid())
                    {
                        errorMessageStr = root.GetString("message");
                        if (errorMessageStr.empty())
                        {
                            errorMessageStr = root.GetString("error");
                            if (errorMessageStr.empty())
                            {
                                errorMessageStr = std::string(result->pszErrBuf);
                            }
                        }
                    }
                }
                putMessage(COD_REQUEST_FAILED, errorMessageStr.c_str());
                return false;
            }

            return ret;
        }

        Options addAuthHeaders(const std::string &url, const Options &options)
        {
            Options out(options);
            auto auth = AuthStore::authHeader(url);
            if (!auth.empty())
            {
                auto headers = options.asString("HEADERS");
                if (!headers.empty())
                {
                    auth += "\r\n" + headers;
                    out.add("HEADERS", auth);
                }
                else
                {
                    out.add("HEADERS", auth);
                }
            }
            return out;
        }

        Options getGDALHeaders(const std::string &url)
        {
            Options out;
            std::string headers = "Accept: */*";
            std::string auth = AuthStore::authHeader(url);
            if (!auth.empty())
            {
                headers += "\r\n";
                headers += auth;
            }
            out.add("HEADERS", headers);
            return out;
        }

        CPLJSONObject uploadFile(const std::string &url, const std::string &filePath,
                                 const Progress &progress, const Options &options)
        {
            resetError();
            auto requestOptions = addAuthHeaders(url, options);
            requestOptions.add("FORM_FILE_PATH", filePath);
            requestOptions.add("FORM_FILE_NAME", "file");
            requestOptions.add("FORM_KEY_0", "name");
            requestOptions.add("FORM_VALUE_0", File::getFileName(filePath));
            requestOptions.add("FORM_ITEM_COUNT", "1");

            Progress progressIn(progress);

            HTTPResultPtr httpResult = httpFetchCallback(url.c_str(), requestOptions.asStringList(),
                                                         ngsGDALProgress, &progressIn,
                                                         nullptr, nullptr);
            if (nullptr == httpResult)
            {
                putMessage(COD_REQUEST_FAILED, _("Unexpected error"));
                return CPLJSONObject();
            }
            if (httpResult->nStatus != 0 || httpResult->pszErrBuf != nullptr)
            {
                std::string errorMessageStr(httpResult->pszErrBuf);
                CPLJSONDocument resultDoc;
                if (resultDoc.LoadMemory(httpResult->pabyData, httpResult->nDataLen))
                {
                    CPLJSONObject root = resultDoc.GetRoot();
                    if (root.IsValid())
                    {
                        errorMessageStr = root.GetString("message");
                        if (errorMessageStr.empty())
                        {
                            errorMessageStr = root.GetString("error");
                            if (errorMessageStr.empty())
                            {
                                errorMessageStr = std::string(httpResult->pszErrBuf);
                            }
                        }
                    }
                }
                putMessage(COD_REQUEST_FAILED, errorMessageStr.c_str());
                return CPLJSONObject();
            }

            CPLJSONObject result;
            CPLJSONDocument fileJson;
            if (fileJson.LoadMemory(httpResult->pabyData, httpResult->nDataLen))
            {
                result = fileJson.GetRoot();
            }
            else
            {
                putMessage(COD_REQUEST_FAILED, _("Upload file %s failed"), filePath.c_str());
            }
            return result;
        }
    } // http

} // ngs
