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
#ifndef NGSNOTIFY_H
#define NGSNOTIFY_H

#include <string>
#include <vector>

#include "ngstore/api.h"

namespace ngs {

/**
 * @brief The Notify class to subscribe/unsubscribe to various library
 * notifications.
 */
class Notify
{
public:
    static Notify& instance();

public:
    void addNotifyReceiver(ngsNotifyFunc function, int notifyTypes);
    void deleteNotifyReceiver(ngsNotifyFunc function);
    void onNotify(const std::string &uri, enum ngsChangeCode operation) const;

private:
    Notify() = default;
    ~Notify() = default;
    Notify(Notify const&) = delete;
    Notify& operator= (Notify const&) = delete;

private:
    typedef struct _notifyData {
        ngsNotifyFunc notifyFunc;
        int notifyTypes;
    } notifyData;

    std::vector<notifyData> m_notifyReceivers;
};

} // namespace ngs

#endif // NGSNOTIFY_H
