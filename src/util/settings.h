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

#ifndef NGSSETTINGS_H
#define NGSSETTINGS_H

#include "jsondocument.h"

namespace ngs {

/**
 * @brief The Settings class provides persistent platform-independent library settings.
 */
class Settings
{
public:
    Settings(const CPLString& path);
    ~Settings();
/*
    // settres
    void set(const char* path, bool val);
    void set(const char* path, double val);
    void set(const char* path, int val);
    void set(const char* path, long val);
    void set(const char* path, const char* val);

    // getters
    bool getBool(const char* path, bool defaultVal) const;
    double getDouble(const char* path, double defaultVal) const;
    int getInteger(const char* path, int defaultVal) const;
    long getLong(const char* path, long defaultVal) const;
    CPLString get(const char* path, const char* defaultVal) const;
*/
    // settings
    bool save();
private:
    JSONObject getObjectByPath(const char* path) const;
private:
    JSONDocument m_settings;
    JSONObject m_root;
    CPLString m_path;
    bool m_hasChanges;
};

}

#endif // NGSSETTINGS_H
