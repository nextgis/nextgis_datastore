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
#ifndef NGSOPTIONS_H
#define NGSOPTIONS_H

// gdal
#include "cpl_string.h"

// std
#include <map>
#include <memory>
#include <string>

namespace ngs {

class Options
{
public:
    Options() = default;
    explicit Options(char **options);

    std::string asString(const std::string &key,
                         const std::string &defaultOption = "") const;
    bool asBool(const std::string &key, bool defaultOption = true) const;
    int asInt(const std::string &key, int defaultOption = 0) const;
    long asLong(const std::string &key, long defaultOption = 0) const;
    double asDouble(const std::string &key, double defaultOption = 0.0) const;
    CPLStringList asCPLStringList() const;

    void add(const std::string &key, const std::string &value);
    void add(const std::string &key, const char *value);
    void add(const std::string &key, long value);
    void add(const std::string &key, GIntBig value);
    void add(const std::string &key, bool value);
    void remove(const std::string &key);
    bool empty() const;
    std::map< std::string, std::string >::const_iterator begin() const;
    std::map< std::string, std::string >::const_iterator end() const;

    void append(const Options &other);
    std::string operator[](std::string key) const;

    bool hasKey(const std::string &key) const;

protected:
    std::map<std::string, std::string> m_options;
};

unsigned char getNumberThreads();

}

#endif // NGSOPTIONS_H
