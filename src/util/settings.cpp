/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author:   Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2017-2019 NextGIS, <info@nextgis.com>
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

#include "settings.h"
#include "stringutil.h"

#include "catalog/file.h"
#include "catalog/folder.h"

#include "ngstore/version.h"

// gdal
#include "cpl_conv.h"

namespace ngs {

constexpr const char *SETTINGS_FILE = "settings";
constexpr const char *SETTINGS_FILE_EXT = "json";

constexpr const char *HTTP_TIMEOUT = "20";
constexpr const char *HTTP_CONN_TIMEOUT = "10";
constexpr const char *HTTP_MAX_RETRY = "3";
constexpr const char *HTTP_RETRY_DELAY = "5";
constexpr const char *HTTP_USE_GZIP = "YES";

#if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
constexpr const char *CACHEMAX = "4";
#elif __ANDROID__
constexpr const char *CACHEMAX = "4";
#else
constexpr const char *CACHEMAX = "64";
#endif

Settings::Settings() : m_hasChanges(false)
{
    const char* settingsPath = CPLGetConfigOption("NGS_SETTINGS_PATH", nullptr);
    if(!Folder::isExists(settingsPath)) {
        Folder::mkDir(settingsPath);
    }
    m_path = File::formFileName(settingsPath, SETTINGS_FILE, SETTINGS_FILE_EXT);
    if(Folder::isExists(m_path)) {
        m_settings.Load(m_path);
    }
    m_root = m_settings.GetRoot();

    init();
}

Settings::~Settings()
{
    save();
}

Settings &Settings::instance()
{
    static Settings s;
    return s;
}

std::string Settings::getConfigOption(const std::string &key,
                                      const std::string &defaultVal)
{
    return CPLGetConfigOption(key.c_str(), defaultVal.c_str());
}

void Settings::set(const std::string &path, bool val)
{
    m_root.Set(path, val);
    m_hasChanges = true;
}

void Settings::set(const std::string &path, double val)
{
    m_root.Set(path, val);
    m_hasChanges = true;
}

void Settings::set(const std::string &path, int val)
{
    m_root.Set(path, val);
    m_hasChanges = true;
}

void Settings::set(const std::string &path, long val)
{
    m_root.Set(path, static_cast<GInt64>(val));
    m_hasChanges = true;
}

void Settings::set(const std::string &path, const std::string &val)
{
    m_root.Set(path, val);
    m_hasChanges = true;

    if(compare(path, "common/cachemax")) {
        CPLSetConfigOption("GDAL_CACHEMAX", val.c_str());
    }

    if(compare(path, "http/useragent")) {
        CPLSetConfigOption("GDAL_HTTP_USERAGENT", val.c_str());
    }

    if(compare(path, "http/use_gzip")) {
        CPLSetConfigOption("CPL_CURL_GZIP", val.c_str());
    }

    if(compare(path, "http/timeout")) {
        CPLSetConfigOption("GDAL_HTTP_TIMEOUT", val.c_str());
    }

    if(compare(path, "http/conn_timeout")) {
        CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", val.c_str());
    }

    if(compare(path, "http/max_retry")) {
        CPLSetConfigOption("GDAL_HTTP_MAX_RETRY", val.c_str());
    }

    if(compare(path, "http/retry_delay")) {
        CPLSetConfigOption("GDAL_HTTP_RETRY_DELAY", val.c_str());
    }

    if(compare(path, "gdal/CPL_VSIL_ZIP_ALLOWED_EXTENSIONS")) {
        CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", val.c_str());
    }

    if(compare(path, "common/zip_encoding")) {
        CPLSetConfigOption("CPL_ZIP_ENCODING", val.c_str());
    }
}

bool Settings::getBool(const std::string &path, bool defaultVal) const
{
    return m_root.GetBool(path, defaultVal);
}

double Settings::getDouble(const std::string &path, double defaultVal) const
{
    return m_root.GetDouble(path, defaultVal);
}

int Settings::getInteger(const std::string &path, int defaultVal) const
{
    return m_root.GetInteger(path, defaultVal);
}

long Settings::getLong(const std::string &path, long defaultVal) const
{
    return m_root.GetLong(path, defaultVal);
}

const std::string Settings::getString(const std::string &path,
                                      const std::string &defaultVal) const
{
    return m_root.GetString(path, defaultVal);
}

bool Settings::save()
{
    if(m_hasChanges) {
        bool result = m_settings.Save(m_path);
        if(result) {
            m_hasChanges = false;
        }

        return result;
    }
    return true;
}

void Settings::init()
{
    CPLSetConfigOption("GDAL_CACHEMAX", getString("common/cachemax",
                                                  CACHEMAX).c_str());

    CPLSetConfigOption("GDAL_HTTP_USERAGENT", getString("http/useragent",
                                                        NGS_USERAGENT).c_str());
    CPLSetConfigOption("CPL_CURL_GZIP", getString("http/use_gzip",
                                                  HTTP_USE_GZIP).c_str());
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", getString("http/timeout",
                                                      HTTP_TIMEOUT).c_str());
    CPLSetConfigOption("GDAL_DRIVER_PATH", "disabled");

    CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT",
                       getString("http/conn_timeout", HTTP_CONN_TIMEOUT).c_str());

    CPLSetConfigOption("GDAL_HTTP_MAX_RETRY",
                       getString("http/max_retry", HTTP_MAX_RETRY).c_str());

    CPLSetConfigOption("GDAL_HTTP_RETRY_DELAY",
                       getString("http/retry_delay", HTTP_RETRY_DELAY).c_str());

    CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS",
                       getString("gdal/CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", ".ngmd").c_str());

    CPLDebug("ngstore", "ZIP support %s", CPLGetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", ""));

    CPLSetConfigOption("CPL_ZIP_ENCODING",
                       getString("common/zip_encoding", "CP866").c_str());

    CPLDebug("ngstore", "HTTP user agent set to: %s", NGS_USERAGENT);

    CPLSetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "OFF");
    CPLSetConfigOption("GDAL_VALIDATE_OPEN_OPTIONS", "OFF");
}

} // ngs
