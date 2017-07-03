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

#include <time.h>
#include <vector>

#include "cpl_string.h"

#include "util/options.h"

namespace ngs {

class IAuth {
public:
    virtual ~IAuth() = default;
    virtual const char* url() const = 0;
    virtual const char* header() = 0;
    virtual Options description() const = 0;
};

typedef std::unique_ptr<IAuth> AuthUPtr;

class AuthStore
{
public:
    static bool addAuth(const char *url, const Options& options);
    static AuthStore& instance();

public:
    void addAuth(IAuth* auth);
    void deleteAuth(const char* url);
    const char* getAuthHeader(const char* url);
    Options description(const char* url) const;

private:
    AuthStore() = default;
    ~AuthStore() = default;
    AuthStore(AuthStore const&) = delete;
    AuthStore& operator= (AuthStore const&) = delete;

private:
    std::vector<AuthUPtr> m_auths;
};

class AuthBearer : public IAuth {

public:
    AuthBearer(const CPLString& url, const CPLString& clientId,
               const CPLString& tokenServer, const CPLString& accessToken,
               const CPLString& updateToken, int expiresIn);
    virtual ~AuthBearer() = default;
    virtual const char* url() const override { return m_url; }
    virtual const char* header() override;
    virtual Options description() const override;

private:
    CPLString m_url;
    CPLString m_clientId;
    CPLString m_accessToken;
    CPLString m_updateToken;
    CPLString m_tokenServer;
    int m_expiresIn;
    time_t m_lastCheck;
};

} // namespace ngs

#endif // NGSAUTHSTORE_H
