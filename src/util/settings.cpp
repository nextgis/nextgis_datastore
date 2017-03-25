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
        m_settings.load(m_path);
    m_root = m_settings.getRoot();
}

Settings::~Settings()
{
    save();
}

void Settings::set(const char *path, bool val)
{
    m_root.set(path, val);
}

void Settings::set(const char *path, double val)
{
    m_root.set(path, val);
}

void Settings::set(const char *path, int val)
{
    m_root.set(path, val);
}

void Settings::set(const char *path, long val)
{
    m_root.set(path, val);
}

void Settings::set(const char *path, const char *val)
{
    m_root.set(path, val);
}

bool Settings::getBool(const char *path, bool defaultVal) const
{
    return m_root.getBool(path, defaultVal);
}

double Settings::getDouble(const char *path, double defaultVal) const
{
    return m_root.getDouble(path, defaultVal);
}

int Settings::getInteger(const char *path, int defaultVal) const
{
    return m_root.getInteger(path, defaultVal);
}

long Settings::getLong(const char *path, long defaultVal) const
{
    return m_root.getLong(path, defaultVal);
}

const char *Settings::getString(const char *path, const char *defaultVal) const
{
    return m_root.getString(path, defaultVal);
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

        return m_settings.save(m_path);
    }
    return true;
}

}
