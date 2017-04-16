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
#include "util/progress.h"

#include "cpl_string.h"

namespace ngs {

Progress::Progress(ngsProgressFunc progressFunc, void *progressArguments ) :
    m_progressFunc(progressFunc),
    m_progressArguments(progressArguments)
{

}

bool Progress::onProgress(ngsErrorCodes status, double complete,
                          const char *format, ...) const
{
    if(nullptr == m_progressFunc)
        return true; // No cancel from user
    va_list args;
    CPLString message;
    va_start( args, format );
    message.vPrintf( format, args );
    va_end( args );
    return m_progressFunc(status, complete, message, m_progressArguments) == 1;
}

}
