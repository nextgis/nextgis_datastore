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
#include "authstore.h"

#include "cpl_http.h"
#include "cpl_json.h"

#include "api_priv.h"
#include "util/error.h"
#include "util/notify.h"

namespace ngs {

/**
 * @brief The HTTPAuthBasic class Basic HTTP authorisation.
 */
class HTTPAuthBasic : public IHTTPAuth {

public:
    explicit HTTPAuthBasic(const std::string &login, const std::string &password);
    virtual ~HTTPAuthBasic() = default;
    virtual std::string header() override { return "Authorization: Basic " + m_basicAuth; }
    virtual Properties properties() const override;

private:
    std::string m_basicAuth;
};

HTTPAuthBasic::HTTPAuthBasic(const std::string &login, const std::string &password)
{
    std::string str = login + ":" + password;
    char *encodedStr = CPLBase64Encode(static_cast<int>(str.size()),
                                       reinterpret_cast<const GByte*>(str.data()));
    m_basicAuth = encodedStr;
    CPLFree(encodedStr);
}

Properties HTTPAuthBasic::properties() const
{
    Properties out;
    out.add("type", "basic");
    out.add("basic", m_basicAuth);
    return out;
}

/**
 * @brief The HTTPAuthBearer class
 */
class HTTPAuthBearer : public IHTTPAuth {

public:
    explicit HTTPAuthBearer(const std::string &url, const std::string &clientId,
                            const std::string &tokenServer, const std::string &accessToken,
                            const std::string &updateToken, int expiresIn,
                            time_t lastCheck);
    virtual ~HTTPAuthBearer() = default;
    virtual std::string header() override;
    virtual Properties properties() const override;

private:
    std::string m_url;
    std::string m_clientId;
    std::string m_accessToken;
    std::string m_updateToken;
    std::string m_tokenServer;
    int m_expiresIn;
    time_t m_lastCheck;
};

HTTPAuthBearer::HTTPAuthBearer(const std::string &url, const std::string &clientId,
                               const std::string &tokenServer, const std::string &accessToken,
                               const std::string &updateToken, int expiresIn,
                               time_t lastCheck) : IHTTPAuth(),
    m_url(url),
    m_clientId(clientId),
    m_accessToken(accessToken),
    m_updateToken(updateToken),
    m_tokenServer(tokenServer),
    m_expiresIn(expiresIn),
    m_lastCheck(lastCheck)
{

}

Properties HTTPAuthBearer::properties() const
{
    Properties out;
    out.add("type", "bearer");
    out.add("clientId", m_clientId);
    out.add("accessToken", m_accessToken);
    out.add("updateToken", m_updateToken);
    out.add("tokenServer", m_tokenServer);
    out.add("expiresIn", std::to_string(m_expiresIn));
    return out;
}

std::string HTTPAuthBearer::header()
{
    // 1. Check if expires if not return current access token
    time_t now = time(nullptr);
    double seconds = difftime(now, m_lastCheck);
    if(seconds < m_expiresIn) {
        CPLDebug("ngstore", "Token is not expired. Url: %s", m_url.c_str());
        return "Authorization: Bearer " + m_accessToken;
    }

    // 2. Try to update token
    CPLStringList requestOptions;
    requestOptions.AddNameValue("CUSTOMREQUEST", "POST");
    requestOptions.AddNameValue("POSTFIELDS",
                                CPLSPrintf("grant_type=refresh_token&client_id=%s&refresh_token=%s",
                                           m_clientId.c_str(),
                                           m_updateToken.c_str()));

    CPLHTTPResult *result = CPLHTTPFetch(m_tokenServer.c_str(), requestOptions);

    if(result->nStatus != 0 || result->pszErrBuf != nullptr) {
        CPLHTTPDestroyResult( result );
        CPLDebug("ngstore", "Failed to refresh token. Return last not expired. Url: %s",
                 m_url.c_str());
        return "Authorization: Bearer " + m_accessToken;
    }

    CPLJSONDocument resultJson;
    if(!resultJson.LoadMemory(result->pabyData, result->nDataLen)) {
        CPLHTTPDestroyResult( result );
        CPLDebug("ngstore", "Token is expired. Url: %s", m_url.c_str());
        Notify::instance().onNotify(m_url, CC_TOKEN_EXPIRED);
        return "expired";
    }
    CPLHTTPDestroyResult( result );

    // 4. Save new update and access tokens
    CPLJSONObject root = resultJson.GetRoot();
    if(!EQUAL(root.GetString("error", "").c_str(), "")) {
        CPLDebug("ngstore", "Token is expired. Url: %s.Error: %s.", m_url.c_str(),
                 root.GetString("error", "").c_str());
        Notify::instance().onNotify(m_url, CC_TOKEN_EXPIRED);
        return "expired";
    }

    m_accessToken = root.GetString("access_token", m_accessToken);
    m_updateToken = root.GetString("refresh_token", m_updateToken);
    m_expiresIn = root.GetInteger("expires_in", m_expiresIn);
    m_lastCheck = now;

    // 5. Return new Auth Header
    CPLDebug("ngstore", "Token updated. Url: %s", m_url.c_str());
    Notify::instance().onNotify(m_url, CC_TOKEN_CHANGED);

    return "Authorization: Bearer " + m_accessToken;
}

//------------------------------------------------------------------------------
// AuthStore
//------------------------------------------------------------------------------

AuthStore &AuthStore::instance()
{
    static AuthStore s;
    return s;
}

bool AuthStore::authAdd(const std::string &url, const Options &options)
{
    if(options["type"] == "bearer") {
        int expiresIn = std::stoi(options["expiresIn"]);
        std::string clientId = options["clientId"];
        std::string tokenServer = options["tokenServer"];
        std::string accessToken = options["accessToken"];
        std::string updateToken = options["updateToken"];
        time_t lastCheck = 0;
        if(expiresIn == -1) {
            std::string postPayload =
                    CPLSPrintf("grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s",
                               options["code"].c_str(),
                               options["redirectUri"].c_str(), clientId.c_str());

            CPLStringList requestOptions;
            requestOptions.AddNameValue("CUSTOMREQUEST", "POST");
            requestOptions.AddNameValue("POSTFIELDS", postPayload.c_str());

            time_t now = time(nullptr);
            CPLJSONDocument fetchToken;
            bool result = fetchToken.LoadUrl(tokenServer, requestOptions);
            if(!result) {
                CPLDebug("ngstore", "Failed to get tokens");
                return false;
            }

            CPLJSONObject root = fetchToken.GetRoot();
            accessToken = root.GetString("access_token", accessToken);
            updateToken = root.GetString("refresh_token", updateToken);
            expiresIn = root.GetInteger("expires_in", expiresIn);
            lastCheck = now;
        }

        IHTTPAuth *auth = new HTTPAuthBearer(url, clientId, tokenServer,
                                             accessToken, updateToken,
                                             expiresIn, lastCheck);
        instance().add(url, auth);
        return true;
    }
    else if(options["type"] == "basic")
    {
        IHTTPAuth *auth = new HTTPAuthBasic(options["login"], options["password"]);
        instance().add(url, auth);
    }
    return false;
}

void AuthStore::authRemove(const std::string &url)
{
    instance().remove(url);
}

Properties AuthStore::authProperties(const std::string &url)
{
    return instance().properties(url);
}

std::string AuthStore::authHeader(const std::string &url)
{
    return instance().header(url);
}

void AuthStore::add(const std::string &url, IHTTPAuth *auth)
{
    m_auths[url] = auth;
}

void AuthStore::remove(const std::string &url)
{
    m_auths.erase(url);
}

Properties AuthStore::properties(const std::string &url)
{
    if(m_auths[url] == nullptr) {
        return {};
    }
    return m_auths[url]->properties();
}

std::string AuthStore::header(const std::string &url) const
{
    for(auto pair : m_auths) {
        if(STARTS_WITH_CI(url.c_str(), pair.first.c_str())) {
            return pair.second->header();
        }
    }
    return "";
}

} // namespace ngs
