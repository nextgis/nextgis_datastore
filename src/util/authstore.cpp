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

#include "api_priv.h"
#include "util/error.h"
#include "util/jsondocument.h"
#include "util/notify.h"

namespace ngs {

//------------------------------------------------------------------------------
// AuthStore
//------------------------------------------------------------------------------

bool AuthStore::addAuth(const char *url, const Options &options)
{
    if(EQUAL(options.stringOption("TYPE"), "bearer")) {
        IAuth* newAuth = new AuthBearer(url,
                                        options.stringOption("CLIENT_ID"),
                                        options.stringOption("TOKEN_SERVER"),
                                        options.stringOption("ACCESS_TOKEN"),
                                        options.stringOption("REFRESH_TOKEN"),
                                        options.intOption("EXPIRES_IN"));
        AuthStore::instance().addAuth(newAuth);
        return true;
    }
    return false;
}

AuthStore &AuthStore::instance()
{
    static AuthStore n;
    return n;
}

void AuthStore::addAuth(IAuth* auth)
{
    m_auths.push_back(AuthUPtr(auth));
}

void AuthStore::deleteAuth(const char* url)
{
    for(auto auth = m_auths.begin(); auth != m_auths.end(); ++auth) {
        if(EQUAL((*auth)->url(), url)) {
            m_auths.erase(auth);
            return;
        }
    }
}

const char *AuthStore::getAuthHeader(const char *url)
{
    for(auto auth = m_auths.begin(); auth != m_auths.end(); ++auth) {
        const char* baseUrl = (*auth)->url();
        if(EQUALN(baseUrl, url, CPLStrnlen(baseUrl, 512))) {
            return (*auth)->header();
        }
    }
    return "";
}

Options AuthStore::description(const char *url) const
{
    for(auto auth = m_auths.begin(); auth != m_auths.end(); ++auth) {
        if(EQUAL((*auth)->url(), url)) {
            return (*auth)->description();
        }
    }
    return Options();
}

//------------------------------------------------------------------------------
// AuthBearer
//------------------------------------------------------------------------------
AuthBearer::AuthBearer(const CPLString &url, const CPLString &clientId, const CPLString &tokenServer,
                       const CPLString &accessToken, const CPLString &updateToken,
                       int expiresIn) : IAuth(),
    m_url(url),
    m_clientId(clientId),
    m_accessToken(accessToken),
    m_updateToken(updateToken),
    m_tokenServer(tokenServer),
    m_expiresIn(expiresIn),
    m_lastCheck(0)
{

}

Options AuthBearer::description() const
{
    Options out;
    out.addOption("TYPE", "bearer");
    out.addOption("ACCESS_TOKEN", m_accessToken);
    out.addOption("UPDATE_TOKEN", m_updateToken);
    out.addOption("TOKEN_SERVER", m_tokenServer);
    out.addOption("TOKEN_SERVER", m_tokenServer);
    out.addOption("URL", m_url);
    out.addOption("EXPIRES_IN", CPLSPrintf("%d", m_expiresIn));

    return out;
}

const char *AuthBearer::header()
{
    // 1. Check if expires if not return current access token
    time_t now = time(nullptr);
    double seconds = difftime(now, m_lastCheck);
    if(seconds < m_expiresIn) {
        CPLDebug("ngstore", "Token is not expired. Url: %s", m_url.c_str());
        return CPLSPrintf("Authorization: bearer %s", m_accessToken.c_str());
    }
    // 2. Try to update token

    Options options;
    options.addOption("CUSTOMREQUEST", "POST");
    options.addOption("POSTFIELDS", CPLSPrintf("grant_type=refresh_token&client_id=%s&refresh_token=%s",
                                     m_clientId.c_str(), m_updateToken.c_str()));
    options.addOption("CONNECTTIMEOUT", "5");
    options.addOption("TIMEOUT", "15");
    options.addOption("MAX_RETRY", "5");
    options.addOption("RETRY_DELAY", "5");
    auto optionsPtr = options.getOptions();
    CPLHTTPResult* result = CPLHTTPFetch(m_tokenServer, optionsPtr.get());
    if(result->nStatus != 0 || result->nHTTPResponseCode >= 400) {
        // 3. If failed to update - return "expired"
        errorMessage(COD_UPDATE_FAILED, _("Failed to update token. Url: %s"),
                     m_url.c_str());

        CPLHTTPDestroyResult( result );

        Notify::instance().onNotify(m_url, CC_TOKEN_EXPIRED);

        return "expired";
    }

    JSONDocument resultJson;
    if(!resultJson.load(result->pabyData, result->nDataLen)) {
        CPLHTTPDestroyResult( result );

        Notify::instance().onNotify(m_url, CC_TOKEN_EXPIRED);

        return "expired";
    }
    CPLHTTPDestroyResult( result );

    // 4. Save new update and access tokens
    JSONObject root = resultJson.getRoot();
    m_accessToken = root.getString("access_token", m_accessToken);
    m_updateToken = root.getString("refresh_token", m_updateToken);
    m_expiresIn = root.getInteger("expires_in", m_expiresIn);
    m_lastCheck = now;

    // 5. Return new Auth Header
    return CPLSPrintf("Authorization: bearer %s", m_accessToken.c_str());
}

} // namespace ngs
