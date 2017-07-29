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

Progress::Progress(ngsProgressFunc progressFunc, void* progressArguments ) :
    m_progressFunc(progressFunc),
    m_progressArguments(progressArguments),
    m_totalSteps(1),
    m_step(0)
{

}

bool Progress::onProgress(ngsCode status, double complete,
                          const char* format, ...) const
{
    if(nullptr == m_progressFunc)
        return true; // No cancel from user
    va_list args;
    CPLString message;
    va_start( args, format );
    message.vPrintf( format, args );
    va_end( args );

    double newComplete = complete / m_totalSteps + 1.0 / m_totalSteps * m_step;
    if(status == COD_FINISHED && newComplete < 1.0) {
        status = COD_IN_PROCESS;
    }
    return m_progressFunc(status, newComplete, message, m_progressArguments) == 1;
}

int onGDALProgress(double complete, const char *message,  void *progressArg) {
    Progress* progress = static_cast<Progress*>(progressArg);
    if(nullptr == progress)
        return 1;
    ngsCode status = COD_FINISHED;
    if(complete < 1.0)
        status = COD_IN_PROCESS;
    return progress->onProgress(status, complete, message) ? 1 : 0;
}

}
