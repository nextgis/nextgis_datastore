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
#include "options.h"

#include "cpl_multiproc.h"

namespace ngs {

constexpr short MAX_OPTION_LEN = 255;

Options::Options(char **options)
{
    if(nullptr != options) {
        int i = 0;
        while (options[i] != nullptr) {
            const char* option = options[i];
            size_t len = CPLStrnlen(option, MAX_OPTION_LEN);
            CPLString key, value;
            for(size_t j = 0; j < len; ++j) {
                if(option[j] == '=' || option[j] == ':' ) {
                    value = option + 1 + j;
                    break;
                }
                key += option[j];
            }
            m_options[key] = value;
            i++;
        }
    }
}

const CPLString &Options::stringOption(const char *key,
                                          const CPLString &defaultOption) const
{
    auto it = m_options.find(key);
    if(it == m_options.end())
        return defaultOption;
    return it->second;
}

bool Options::boolOption(const char *key, bool defaultOption) const
{
    auto it = m_options.find(key);
    if(it == m_options.end())
        return defaultOption;

    if(it->second.empty())
        return false;
    else if(EQUAL(it->second, "OFF"))
        return false;
    else if(EQUAL(it->second, "FALSE"))
        return false;
    else if(EQUAL(it->second, "NO"))
        return false;
    else if(EQUAL(it->second, "0"))
        return false;
    return true;
}

int Options::intOption(const char *key, int defaultOption) const
{
    auto it = m_options.find(key);
    if(it == m_options.end())
        return defaultOption;
    return atoi(it->second);
}

double Options::doubleOption(const char *key, double defaultOption) const
{
    auto it = m_options.find(key);
    if(it == m_options.end())
        return defaultOption;
    return CPLAtofM(it->second);
}

OptionsArrayUPtr Options::getOptions() const
{
    char** options = nullptr;
    for(auto it = m_options.begin(); it != m_options.end(); ++it) {
        options = CSLAddNameValue(options, it->first, it->second);
    }
    return OptionsArrayUPtr(options, CSLDestroy);
}

void Options::removeOption(const char *key)
{
    auto it = m_options.find(key);
    if(it != m_options.end())
        m_options.erase(it);
}

unsigned char getNumberThreads()
{
    unsigned char numThreads = static_cast<unsigned char>(CPLGetNumCPUs());

    const char* numThreadsStr = CPLGetConfigOption("GDAL_NUM_THREADS", NULL);
    if(numThreadsStr) {
        if(!EQUAL(numThreadsStr, "ALL_CPUS")) {
            numThreads = static_cast<unsigned char>(atoi(numThreadsStr));
        }
    }

    if(numThreads < 1) {
        numThreads = 1;
    }

    return numThreads;
}

}

