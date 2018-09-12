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
#ifndef NGSAUTHSTORE_H
#define NGSAUTHSTORE_H

#include "util/options.h"

namespace ngs {

class AuthStore
{
public:
    static bool addAuth(const std::string &url, const Options &options);
    static void deleteAuth(const std::string &url);
    static Options description(const std::string &url);

private:
    AuthStore() = default;
    ~AuthStore() = default;
    AuthStore(AuthStore const&) = delete;
    AuthStore& operator= (AuthStore const&) = delete;
};

} // namespace ngs

#endif // NGSAUTHSTORE_H
