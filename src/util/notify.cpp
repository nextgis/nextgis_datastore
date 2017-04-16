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
#include "notify.h"

namespace ngs {

Notify &Notify::instance()
{
    static Notify n;
    return n;
}

void Notify::addNotifyReceiver(ngsNotifyFunc function, int notifyTypes)
{
    for(auto it = m_notifyReceivers.begin(); it != m_notifyReceivers.end(); ++it) {
        if((*it).notifyFunc == function) {
            (*it).notifyTypes = notifyTypes;
            return;
        }
    }
    m_notifyReceivers.push_back({function, notifyTypes});
}

void Notify::deleteNotifyReceiver(ngsNotifyFunc function)
{
    for(auto it = m_notifyReceivers.begin(); it != m_notifyReceivers.end(); ++it) {
        if((*it).notifyFunc == function) {
            m_notifyReceivers.erase(it);
            return;
        }
    }
}

void Notify::onNotify(const char *uri, ngsChangeCode operation) const
{
    for(auto it = m_notifyReceivers.begin(); it != m_notifyReceivers.end(); ++it) {
        if((*it).notifyTypes & operation) {
            (*it).notifyFunc(uri, operation);
        }
    }
}


}
