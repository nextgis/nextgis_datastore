/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author:   Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2017 NextGIS, <info@nextgis.com>
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
#include "catalog/folder.h"

namespace ngs {

constexpr const char * SETTINGS_FILE = "settings";
constexpr const char * SETTINGS_FILE_EXT = "json";

Settings::Settings() : m_hasChanges(false)
{
    const char* settingsPath = CPLGetConfigOption("NGS_SETTINGS_PATH", nullptr);
    m_path = CPLFormFilename(settingsPath, SETTINGS_FILE, SETTINGS_FILE_EXT);
    if(Folder::isExists(m_path))
        m_settings.Load(m_path);
    m_root = m_settings.GetRoot();
}

Settings::~Settings()
{
    save();
}

void Settings::set(const char *path, bool val)
{
    m_root.Set(path, val);
}

void Settings::set(const char *path, double val)
{
    m_root.Set(path, val);
}

void Settings::set(const char *path, int val)
{
    m_root.Set(path, val);
}

void Settings::set(const char *path, long val)
{
    m_root.Set(path, val);
}

void Settings::set(const char *path, const char *val)
{
    m_root.Set(path, val);

    if(EQUAL(path, "common/cachemax")) {
        CPLSetConfigOption("GDAL_CACHEMAX", val);
    }

    if(EQUAL(path, "http/useragent")) {
        CPLSetConfigOption("GDAL_HTTP_USERAGENT", val);
    }

    if(EQUAL(path, "http/use_gzip")) {
        CPLSetConfigOption("CPL_CURL_GZIP", val);
    }

    if(EQUAL(path, "http/timeout")) {
        CPLSetConfigOption("GDAL_HTTP_TIMEOUT", val);
    }

    if(EQUAL(path, "gdal/CPL_VSIL_ZIP_ALLOWED_EXTENSIONS")) {
        CPLSetConfigOption("CPL_VSIL_ZIP_ALLOWED_EXTENSIONS", val);
    }

    if(EQUAL(path, "common/zip_encoding")) {
        CPLSetConfigOption("CPL_ZIP_ENCODING", val);
    }
}

bool Settings::getBool(const char *path, bool defaultVal) const
{
    return m_root.GetBool(path, defaultVal);
}

double Settings::getDouble(const char *path, double defaultVal) const
{
    return m_root.GetDouble(path, defaultVal);
}

int Settings::getInteger(const char *path, int defaultVal) const
{
    return m_root.GetInteger(path, defaultVal);
}

long Settings::getLong(const char *path, long defaultVal) const
{
    return m_root.GetLong(path, defaultVal);
}

const char *Settings::getString(const char *path, const char *defaultVal) const
{
    return m_root.GetString(path, defaultVal);
}

bool Settings::save()
{
    if(m_hasChanges) {
        const char* settingsPath = CPLGetConfigOption("NGS_SETTINGS_PATH",
                                                      nullptr);
        if(nullptr == settingsPath)
            return false;

        if(!Folder::isExists(settingsPath)) {
            if(!Folder::mkDir(settingsPath))
                return false;
        }

        return m_settings.Save(m_path);
    }
    return true;
}

}
