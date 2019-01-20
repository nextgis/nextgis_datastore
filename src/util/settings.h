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

#ifndef NGSSETTINGS_H
#define NGSSETTINGS_H

#include "cpl_json.h"

namespace ngs {

/**
 * @brief The Settings class provides persistent platform-independent library
 * settings.
 */
class Settings
{
public:
    static Settings &instance();
    static std::string getConfigOption(const std::string &key,
                                       const std::string &defaultVal = "");

    // settres
    void set(const std::string &path, bool val);
    void set(const std::string &path, double val);
    void set(const std::string &path, int val);
    void set(const std::string &path, long val);
    void set(const std::string &path, const std::string &val);

    // getters
    bool getBool(const std::string &path, bool defaultVal) const;
    double getDouble(const std::string &path, double defaultVal) const;
    int getInteger(const std::string &path, int defaultVal) const;
    long getLong(const std::string &path, long defaultVal) const;
    const std::string getString(const std::string &path,
                                 const std::string &defaultVal) const;

    // settings
    bool save();

private:
    Settings();
    ~Settings();
    Settings(Settings const&) = delete;
    Settings &operator= (Settings const&) = delete;
    void init();

private:
    CPLJSONDocument m_settings;
    CPLJSONObject m_root;
    std::string m_path;
    bool m_hasChanges;
};

}

#endif // NGSSETTINGS_H
