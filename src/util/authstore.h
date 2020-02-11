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
#ifndef NGSAUTHSTORE_H
#define NGSAUTHSTORE_H

#include "util/options.h"

#include <vector>

namespace ngs {

using Properties = Options;

/**
 * @brief The IHTTPAuth class is base class for HTTP Authorization headers
 */
class IHTTPAuth {
public:
    virtual ~IHTTPAuth() = default;
    virtual std::string header() = 0;
    virtual Properties properties() const = 0;
};

using IHTTPAuthPtr = std::shared_ptr<IHTTPAuth>;

/**
 * @brief The AuthStore class. Storage for auth data.
 */
class AuthStore
{
public:
    static bool authAdd(const std::string &url, const Options &options);
    static bool authAdd(const std::vector<std::string> &urls, const Options &options);
    static void authRemove(const std::string &url);
    static Properties authProperties(const std::string &url);
    static std::string authHeader(const std::string &url);

public:
    void add(const std::string &url, IHTTPAuthPtr auth);
    void remove(const std::string &url);
    Properties properties(const std::string &url);
    std::string header(const std::string &url) const;

private:
    AuthStore() = default;
    ~AuthStore() = default;
    AuthStore(AuthStore const&) = delete;
    AuthStore& operator= (AuthStore const&) = delete;

private:
    static AuthStore &instance();

private:
    std::map<std::string, IHTTPAuthPtr> m_auths;
};

} // namespace ngs

#endif // NGSAUTHSTORE_H
