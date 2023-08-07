/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author:   Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019 NextGIS, <info@nextgis.com>
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

#ifndef NGSACCOUNT_H
#define NGSACCOUNT_H

#include <string>
#include <vector>

namespace ngs {

constexpr unsigned short MAX_FEATURES4UNSUPPORTED = 999;
constexpr unsigned short MAX_RASTERSIZE4UNSUPPORTED = 999;

struct UserInfo {
    std::string firstName;
    std::string lastName;
    std::string username;
    std::string guid;
    std::string locale;
};

struct TeamInfo {
    std::string id;
    std::string ownerId;
    std::string webgis;
    std::string startDate;
    std::string endDate;
    std::vector<UserInfo> users;
};

class Account
{
public:
    static Account &instance();

public:   
    std::string firstName() const { return m_firstName; }
    std::string lastName() const { return m_lastName; }
    std::string email() const { return m_email; }
    std::string avatarFilePath() const { return m_avatarPath; }
    std::vector<TeamInfo> teams() const { return m_teams; }
    void exit();
    bool isFunctionAvailable(const std::string &app, const std::string &func) const;
    bool isUserSupported() const { return m_supported; }
    bool isUserAuthorized() const { return m_authorized; }
    bool updateUserInfo();
    bool updateSupportInfo();
    bool updateTeamsInfo();

private:
    bool checkSupported();

private:
    Account();
    ~Account() = default;
    Account(Account const&) = delete;
    Account &operator= (Account const&) = delete;

private:
    std::string m_firstName;
    std::string m_lastName;
    std::string m_email;
    bool m_authorized;
    bool m_supported;
    std::string m_avatarPath;
    std::vector<TeamInfo> m_teams;
};

} // namespace ngs

#endif // NGSACCOUNT_H
