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
#include "fileutil.h"

namespace ngs {

#define SETTINGS_FILE "settings"
#define SETTINGS_FILE_EXT "json"

Settings::Settings(const CPLString& path) : m_hasChanges(false)
{
    m_path = CPLFormFilename(path, SETTINGS_FILE, SETTINGS_FILE_EXT);
    if(checkPathExist(m_path))
        m_settings.load(m_path);
    m_root = m_settings.getRoot();
}

Settings::~Settings()
{
    save();
}

/*bool Settings::getBool(const char *path, bool defaultVal) const
{
    if(nullptr == path)
        return defaultVal;
    JSONObject object = getObjectByPath(path);
    return object.getBool(defaultVal);

    return defaultVal;
}*/

bool Settings::save()
{
    if(m_hasChanges)
        return m_settings.save(m_path);
    return false;
}

}
