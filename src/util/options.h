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

#include <map>
#include <memory>

#include "cpl_string.h"

namespace ngs {

typedef std::unique_ptr<char*, void(*)(char**)> OptionsArrayUPtr;

class Options
{
public:
    Options() = default;
    Options(const Options& options) : m_options(options.m_options) {}
    Options(char** options);
    const CPLString &stringOption(const char * key,
                                     const CPLString & defaultOption = "") const;
    bool boolOption(const char *key, bool defaultOption = true) const;
    int intOption(const char *key, int defaultOption = 0) const;
    double doubleOption(const char *key, double defaultOption = 0.0) const;
    OptionsArrayUPtr getOptions() const;
    void addOption(const char *key, const char * value) { m_options[key] = value; }
    void removeOption(const char *key);
    bool empty() const { return m_options.empty(); }
    std::map< CPLString, CPLString >::const_iterator begin() const {
        return m_options.begin(); }
    std::map< CPLString, CPLString >::const_iterator end() const {
        return m_options.end(); }

protected:
    std::map< CPLString, CPLString > m_options;
};

}

#endif // NGSOPTIONS_H
