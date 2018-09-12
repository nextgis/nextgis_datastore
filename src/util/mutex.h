/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2018 NextGIS, <info@nextgis.com>
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
#ifndef NGSMUTEX_H
#define NGSMUTEX_H

#include "cpl_multiproc.h"

namespace ngs {

/**
 * @brief The Mutex class
 */
class Mutex {
public:
    Mutex();
    ~Mutex();
    void acquire(double timeout = 1000.0);
    void release();
private:
    CPLMutex *m_mutex;
};

/**
 * @brief The MutexHolder class
 */
class MutexHolder
{
public:
    MutexHolder(const Mutex &mutex, double timeout = 1000.0);
    ~MutexHolder();

protected:
    Mutex &m_mutex;
};


}

#endif // NGSMUTEX_H
