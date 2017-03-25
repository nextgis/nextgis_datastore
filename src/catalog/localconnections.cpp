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

#include "localconnections.h"

#include "folder.h"
#include "ngstore/common.h"
#include "util/jsondocument.h"

#if defined (__APPLE__)
#include <pwd.h>
#endif

namespace ngs {

constexpr const char * LOCAL_CONN_FILE = "connections";
constexpr const char * LOCAL_CONN_FILE_EXT = "json";

LocalConnections::LocalConnections(const ObjectContainer * parent,
                                   const CPLString & path) :
    ObjectContainer(parent, CAT_CONTAINER_LOCALCONNECTION, _("Local connections"),
                    path)
{
    m_path = CPLFormFilename(path, LOCAL_CONN_FILE, LOCAL_CONN_FILE_EXT);
}

bool LocalConnections::hasChildren()
{
    JSONDocument doc;
    if(doc.load (m_path) == ngsErrorCodes::EC_SUCCESS) {
        JSONObject root = doc.getRoot ();
        if(root.getType () == JSONObject::Type::Object) {
            JSONArray connections = root.getArray("connections");
            for(int i = 0; i < connections.size(); ++i) {
                JSONObject connection = connections[i];
                if(connection.getBool("hidden", true))
                    continue;
                CPLString connName = connection.getString("name", "");
                CPLString connPath = connection.getString("path", "");
                m_children.push_back(ObjectPtr(new Folder(this, connName, connPath)));
            }
        }
    }
    else {
       JSONObject root = doc.getRoot ();
       JSONArray connections;
       std::vector<std::pair<const char*, const char*>> connectionPaths;
#if defined(_WIN32)
       char testLetter[3];
       testLetter[1] = ':';
       testLetter[2] = 0;
       for(char i = 'A'; i <= 'Z'; ++i) {
           testLetter[0] = i;
           VSIStatBufL statl;
           int ret = VSIStatL(testLetter, &statl);
           if(ret == 0) {
               if (VSI_ISDIR(statl.st_mode)) {
                   const char* pathToAdd = CPLSPrintf("%s\\", testLetter);
                   connectionPaths.push_back(std::make_pair(testLetter, pathToAdd));
               }
           }
       }
#elif defined(__APPLE__)
       const char *homeDir = getenv("HOME");

       if(nullptr == homeDir) {
           struct passwd* pwd = getpwuid(getuid());
           if (pwd)
              homeDir = pwd->pw_dir;
       }
       connectionPaths.push_back(std::make_pair(_("Home"), homeDir));
       connectionPaths.push_back(std::make_pair(_("Documents"),
                                CPLFormFilename(homeDir, "Documents", nullptr)));
       connectionPaths.push_back(std::make_pair(_("Downloads"),
                                CPLFormFilename(homeDir, "Downloads", nullptr)));
       connectionPaths.push_back(std::make_pair(_("Public"),
                                CPLFormFilename(homeDir, "Public", nullptr)));
#elif defined(__unix__)
       const char *homeDir = getenv("HOME");
       connectionPaths.push_back(std::make_pair(_("Home"), homeDir));
#endif

       for(const auto &connectionPath : connectionPaths ) {
           JSONObject connection;
           connection.add("name", connectionPath.first);
           connection.add("path", connectionPath.second);
           connection.add("hidden", false);

           connections.add(connection);

           m_children.push_back(ObjectPtr(new Folder(this, connectionPath.first,
                                                   connectionPath.second)));
       }
       root.add("connections", connections);
       doc.save(m_path);
    }

    return ObjectContainer::hasChildren();
}

}
