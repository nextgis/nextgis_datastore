/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author:   Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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

#include "account.h"
#include "authstore.h"
#include "error.h"
#include "global.h"
#include "settings.h"
#include "stringutil.h"
#include "url.h"

#include "catalog/file.h"

#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace ngs {

constexpr const char *API_ENDPOINT = "https://my.nextgis.com/api/v1";

constexpr const char *AVATAR_FILE = "avatar";
constexpr const char *KEY_FILE = "public.key";

Account::Account() : m_authorized(false), m_supported(false)
{
    const char *settingsPath = CPLGetConfigOption("NGS_SETTINGS_PATH", nullptr);
    Settings &settings = Settings::instance();
    m_firstName = settings.getString("account/first_name", "no name");
    m_lastName = settings.getString("account/last_name", "no name");
    m_email = settings.getString("account/email", "no email");

    m_avatarPath = File::formFileName(settingsPath, AVATAR_FILE, "");

    m_supported = checkSupported();
}

Account &Account::instance()
{
    static Account a;
    return a;
}

void Account::exit()
{
    m_authorized = m_supported = false;
    m_firstName = m_lastName = m_email = "";
    Settings &settings = Settings::instance();
    settings.set("account/user_id", std::string(""));
    settings.set("account/first_name", std::string("no name"));
    settings.set("account/last_name", std::string("no name"));
    settings.set("account/email", std::string("no email"));
    settings.set("account/sign", std::string(""));
    settings.set("account/start_date", std::string(""));
    settings.set("account/end_date", std::string(""));

    File::deleteFile(m_avatarPath);
}

bool Account::isFunctionAvailable(const std::string &app, const std::string &func) const
{
    ngsUnused(app);
    ngsUnused(func);
    return isUserSupported();
}

bool Account::updateUserInfo()
{
    std::string apiEndpoint(API_ENDPOINT);
    CPLJSONObject root = http::fetchJson(apiEndpoint + "/user_info/");
    if(!root.IsValid()) {
        m_authorized = false;
        return false;
    }

    Settings &settings = Settings::instance();
    settings.set("account/user_id", root.GetString("nextgis_guid"));
    m_firstName = root.GetString("first_name");
    settings.set("account/first_name", m_firstName);
    m_lastName = root.GetString("last_name");
    settings.set("account/last_name", m_lastName);
    m_email = root.GetString("email");
    settings.set("account/email", m_email);

    m_authorized = true;
    // Get avatar
    std::string emailHash = md5(root.GetString("email"));
    return http::getFile(CPLSPrintf("https://www.gravatar.com/avatar/%s?s=64&r=pg&d=robohash",
                             emailHash.c_str()), m_avatarPath);
}

bool Account::updateSupportInfo()
{
    std::string apiEndpoint(API_ENDPOINT);
    CPLJSONObject root = http::fetchJson(apiEndpoint + "/support_info/");
    if(!root.IsValid()) {
        return false;
    }

    bool supported = root.GetBool("supported");

    Settings &settings = Settings::instance();
    settings.set("account/supported", supported);

    if(supported) {
        settings.set("account/sign", root.GetString("sign"));
        settings.set("account/start_date", root.GetString("start_date"));
        settings.set("account/end_date", root.GetString("end_date"));

        // Get key file
        const char *settingsPath = CPLGetConfigOption("NGS_SETTINGS_PATH", nullptr);
        std::string keyFilePath = File::formFileName(settingsPath, KEY_FILE, "");
        return http::getFile(apiEndpoint + "/rsa_public_key/", keyFilePath);
    }

    m_supported = checkSupported();

    return true;
}

static time_t timeFromString(const char *str)
{
    int day(0), month(1), year(1970);
    sscanf(str, "%4d-%2d-%2d", &month, &day, &year);

    struct tm stTm = {0, 0, 0, day, month - 1, year - 1900, 0, 0, 0, 0, nullptr};
    return mktime(&stTm);
}

static bool verifyRSASignature(const unsigned char *originalMessage,
                                  unsigned int messageLength,
                                  const unsigned char *signature,
                                  unsigned int sigLength)
{
    if(nullptr == originalMessage) {
        return errorMessage(_("Message is empty"));
    }

    if(nullptr == signature) {
        return errorMessage(_("Signature is empty"));
    }

    const char *settingsPath = CPLGetConfigOption("NGS_SETTINGS_PATH", nullptr);
    std::string keyFilePath = File::formFileName(settingsPath, KEY_FILE, "");
    FILE *file = VSIFOpen( keyFilePath.c_str(), "r" );
    if( file == nullptr ) {
        return errorMessage(_("Failed open file %s"), keyFilePath.c_str());
    }

    EVP_PKEY *evp_pubkey = PEM_read_PUBKEY(file, nullptr, nullptr, nullptr);
    VSIFClose( file );
    if (!evp_pubkey) {
        return errorMessage(_("Failed PEM_read_PUBKEY"));
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    if (!ctx) {
        EVP_PKEY_free(evp_pubkey);
        return errorMessage(_("Failed PEM_read_PUBKEY"));
    }

    if(!EVP_VerifyInit(ctx, EVP_sha256())) {
        EVP_MD_CTX_destroy(ctx);
        EVP_PKEY_free(evp_pubkey);
        return errorMessage(_("Failed EVP_VerifyInit"));
    }

    if(!EVP_VerifyUpdate(ctx, originalMessage, messageLength)) {
        EVP_MD_CTX_destroy(ctx);
        EVP_PKEY_free(evp_pubkey);
        return errorMessage(_("Failed EVP_VerifyUpdate"));
    }
    int result = EVP_VerifyFinal(ctx, signature, sigLength, evp_pubkey);

    EVP_MD_CTX_destroy(ctx);
    EVP_PKEY_free(evp_pubkey);

    outMessage(result == 1 ? COD_SUCCESS : COD_UNEXPECTED_ERROR,
               "Signature is %s", result == 1 ? "valid" : "invalid");

    return result == 1;
}

bool Account::checkSupported()
{
    Settings &settings = Settings::instance();
    // Read user id, start/end dates, account type
    std::string userId, startDate, endDate, accountType("true"), sign;
    bool supported = settings.getBool("account/supported", false);
    if(!supported) {
        warningMessage(_("Account is not supported"));
        return false;
    }

    userId = settings.getString("account/user_id", "");
    startDate = settings.getString("account/start_date", "");
    endDate = settings.getString("account/end_date", "");
    sign = settings.getString("account/sign", "");

    std::string baMessage = userId + startDate + endDate + accountType;

    GByte *key = reinterpret_cast<GByte*>(CPLStrdup(sign.c_str()));
    int nLength = CPLBase64DecodeInPlace(key);
    std::string baSignature;
    baSignature.assign(reinterpret_cast<const char*>(key),
                       static_cast<size_t>(nLength));
    memset(key, 0, baSignature.size());
    CPLFree(key);

    bool verify = verifyRSASignature(
                reinterpret_cast<const unsigned char*>(baMessage.c_str()),
                static_cast<unsigned int>(baMessage.size()),
                reinterpret_cast<const unsigned char*>(baSignature.c_str()),
                static_cast<unsigned int>(baSignature.size()));
    if(!verify) {
        return false;
    }

    time_t current = time(nullptr);
    time_t start = timeFromString(startDate.c_str());
    time_t end = timeFromString(endDate.c_str());
    bool out = current >= start && current <= end;
    if(!out) {
        warningMessage(_("Account is supported. Verify success. Period expired."));
    }
    return out;
}

} // namespace ngs
