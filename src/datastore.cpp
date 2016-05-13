/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
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
#include "datastore.h"
#include "api.h"

#if defined(WIN32) || defined(_WIN32)
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

#define DEFAULT_CACHE PATH_SEPARATOR ".cache"

using namespace ngs;

DataStore::DataStore(const char *path, const char *cachePath)
{
    if(nullptr != path)
        m_sPath = path;
    if(nullptr == cachePath) {
        if(nullptr != path) {
            m_sCachePath = path;
            m_sCachePath += DEFAULT_CACHE;
        }
    }
    else {
        m_sCachePath = cachePath;
    }
}

int DataStore::create()
{
    if(m_sPath.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;
    return ngsErrorCodes::SUCCESS;
}

int DataStore::open()
{
    if(m_sPath.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;
    return ngsErrorCodes::SUCCESS;
}

int DataStore::openOrCreate()
{
    if(open() != ngsErrorCodes::SUCCESS)
        return create();
    return open();
}
