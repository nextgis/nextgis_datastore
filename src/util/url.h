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
#ifndef NGSURL_H
#define NGSURL_H

#include "ngstore/api.h"

#include "options.h"
#include "progress.h"

// gdal
#include "cpl_json.h"
#include "cpl_http.h"

// std
#include <memory>

namespace ngs {

namespace http {

/**
 * @brief The HTTPResultPtr class
 */
class HTTPResultPtr : public std::shared_ptr<CPLHTTPResult>
{
public:
    HTTPResultPtr(CPLHTTPResult *result);
    HTTPResultPtr();
    HTTPResultPtr &operator=(CPLHTTPResult *result);
    operator CPLHTTPResult*() const;
};


/**
 * @brief The ngsURLRequestResultPtr class
 */
class ngsURLRequestResultPtr : public std::shared_ptr<ngsURLRequestResult>
{
public:
    ngsURLRequestResultPtr(ngsURLRequestResult *result);
    ngsURLRequestResultPtr();
    ngsURLRequestResultPtr &operator=(ngsURLRequestResult *result);
    operator ngsURLRequestResult*() const;
};

//------------------------------------------------------------------------------
ngsURLRequestResult *httpFetch(const std::string &url, const Progress &progress,
                           const Options &options);
bool getFile(const std::string &url, const std::string &path,
             const Progress &progress = Progress(),
             const Options &options = Options());
CPLJSONObject jsonFetch(const std::string &url,
                        const Progress &progress = Progress(),
                        const Options &options = Options());
Options addAuthHeaders(const std::string &url, const Options &options);
Options getGDALHeaders(const std::string &url);
CPLJSONObject uploadFile(const std::string &url, const std::string &filePath,
                         const Progress &progress = Progress(),
                         const Options &options = Options());
bool pushFetchCallback();
bool popFetchCallback();                         
} // namespace http
 
} // namespace ngs

#endif // NGSURL_H
