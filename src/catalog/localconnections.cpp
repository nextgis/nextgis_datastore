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

// gdal
#include "cpl_json.h"

#include "api_priv.h"
#include "file.h"
#include "folder.h"
#include "catalog.h"

#include "ngstore/common.h"
#include "util/settings.h"
#include "util/stringutil.h"

#if defined (__APPLE__)
#include <pwd.h>
#include "TargetConditionals.h"
#endif

namespace ngs {

constexpr const char *LOCAL_CONN_FILE = "connections";
constexpr const char *LOCAL_CONN_FILE_EXT = "json";

LocalConnections::LocalConnections(ObjectContainer * const parent,
                                   const std::string &path) :
    ObjectContainer(parent, CAT_CONTAINER_LOCALCONNECTIONS,
                    _("Local connections"), path)
{
    Folder::mkDir(path, true); // Try to create connections folder
    m_path = File::formFileName(path, LOCAL_CONN_FILE, LOCAL_CONN_FILE_EXT);
}

bool LocalConnections::loadChildren()
{
    if(m_childrenLoaded) {
        return true;
    }

    LocalConnections *parent = const_cast<LocalConnections *>(this);

#if (TARGET_OS_IPHONE == 1) || (TARGET_IPHONE_SIMULATOR == 1)
    // NOTE: For iOS (Mobile?) we don't need to store connections in file as
    // there is the only connection to root directory
    // Add iOS application root path
    std::string homeDir = Settings::getConfigOption("NGS_HOME", getenv("HOME"));
    if(homeDir.empty()) {
        struct passwd* pwd = getpwuid(getuid());
        if (pwd)
           homeDir = pwd->pw_dir;
    }

    m_children.push_back(ObjectPtr(new Folder(parent, "Home", homeDir)));
    m_childrenLoaded = true;
    return true;
#endif // TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR

    CPLJSONDocument doc;
    if(doc.Load (m_path)) {
        CPLJSONObject root = doc.GetRoot();
        if(root.GetType() == CPLJSONObject::Object) {
            CPLJSONArray connections = root.GetArray("connections");
            for(int i = 0; i < connections.Size(); ++i) {
                CPLJSONObject connection = connections[i];
                if(connection.GetBool("hidden", true)) {
                    continue;
                }
                std::string connName = connection.GetString("name");
                std::string connPath = connection.GetString("path");
                m_children.push_back(
                            ObjectPtr(new Folder(parent, connName, connPath)));
            }
        }
    }
    else {
        CPLJSONObject root = doc.GetRoot();
        CPLJSONArray connections("connections");
        std::vector<std::pair<std::string, std::string>> connectionPaths;
#ifdef _WIN32
        char testLetter[3];
        testLetter[1] = ':';
        testLetter[2] = 0;
        for(char i = 'A'; i <= 'Z'; ++i) {
            testLetter[0] = i;
            if(Folder::isDir(testLetter)) {
                const char* pathToAdd = CPLSPrintf("%s\\", testLetter);
                connectionPaths.push_back(std::make_pair(testLetter, pathToAdd));
            }
        }
#elif defined(__APPLE__)
        const char *homeDir = getenv("HOME");

        if(nullptr == homeDir) {
           struct passwd *pwd = getpwuid(getuid());
           if (pwd) {
              homeDir = pwd->pw_dir;
           }
        }
        connectionPaths.push_back(std::make_pair(_("Home"), homeDir));
        connectionPaths.push_back(std::make_pair(_("Documents"),
                                File::formFileName(homeDir, "Documents")));
        connectionPaths.push_back(std::make_pair(_("Downloads"),
                                File::formFileName(homeDir, "Downloads")));
        connectionPaths.push_back(std::make_pair(_("Public"),
                                File::formFileName(homeDir, "Public")));
        connectionPaths.push_back(std::make_pair(_("Volumes"), "/Volumes"));
#elif defined(__ANDROID__)
        std::string ngsNomeDir = Settings::getConfigOption("NGS_HOME");
        if(ngsNomeDir.empty()) {
            const char *homeDir = getenv("HOME");
            if(homeDir) {
                connectionPaths.push_back(std::make_pair(_("Home"), homeDir));
            }
        }
        else {
            connectionPaths.push_back(std::make_pair(_("Home"), ngsNomeDir));
        }

        // https://gist.github.com/PauloLuan/4bcecc086095bce28e22
        // - /storage/sdcard1 //!< Motorola Xoom
        // - /storage/extsdcard //!< Samsung SGS3
        // /storage/sdcard0/external_sdcard // user request
        // /mnt/extsdcard
        // /mnt/sdcard/external_sd //!< Samsung galaxy family
        // /mnt/external_sd
        // /mnt/media_rw/sdcard1 //!< 4.4.2 on CyanogenMod S3
        // /removable/microsd //!< Asus transformer prime
        // /mnt/emmc
        // /storage/external_SD //!< LG
        // /storage/ext_sd //!< HTC One Max
        // /storage/removable/sdcard1 //!< Sony Xperia Z1
        // /data/sdext
        // /data/sdext2
        // /data/sdext3
        // /data/sdext4

        std::vector<std::string> sdCards = {
            "sdcard", "ext_card", "external_sd", "ext_sd", "external",
            "extSdCard", "externalSdCard", "sdcard0", "sdcard1", "sdcard2",
            "usbcard0", "usbcard1", "emmc", "external_SD", "extsdcard", "microsd",
            "removable/sdcard0", "removable/sdcard1", "sdext", "sdext0",
            "sdext1", "sdext2", "sdext3", "sdext4"
        };
        int counter = 2;

        for(auto &sdCard : sdCards) {
            std::string sdCardName = sdCard;
            if(sdCard == "removable/sdcard0") {
                sdCardName = "ExtSD" + std::to_string(counter++);
            }
            if(sdCard == "removable/sdcard1") {
                sdCardName = "ExtSD" + std::to_string(counter++);
            }
            connectionPaths.push_back(std::make_pair(sdCardName, "/mnt/" + sdCard));
            connectionPaths.push_back(std::make_pair("st_" + sdCardName, "/storage/" + sdCard));
            connectionPaths.push_back(std::make_pair("rm_" + sdCardName, "/removable/" + sdCard));
            connectionPaths.push_back(std::make_pair("dt_" + sdCardName, "/data/" + sdCard));
        }

        auto files = Folder::listFiles("/storage");
        
        for(auto &file : files) {
            if(file.size() == 9 && file[4] == '-') {
                connectionPaths.push_back(
                    std::make_pair("ExtSD" + std::to_string(counter++), "/storage/" + file));
            }
        }

        // //https://stackoverflow.com/a/19831753/2901140
        // const char *primarySD = getenv("EXTERNAL_STORAGE");
        // if(primarySD) {
        //     connectionPaths.push_back(std::make_pair(_("SD"), primarySD));
        // }
        // const char *secondarySDs = getenv("SECONDARY_STORAGE");
        // if(secondarySDs) {
        //     char **papszList = CSLTokenizeString2(secondarySDs, ":", 0);
        //     for(int i = 0; i < CSLCount(papszList); ++i) {
        //         connectionPaths.push_back(std::make_pair(_("ExtSD") +
        //             std::to_string(i + 1), papszList[i]));
        //     }
        //     CSLDestroy(papszList);
        // }
#elif defined(__unix__)
        const char *homeDir = getenv("HOME");
        connectionPaths.push_back(std::make_pair(_("Home"), homeDir));
#endif

       for(const auto &connectionPath : connectionPaths ) {
           if(!Folder::isExists(connectionPath.second)) {
               continue;
           }
           CPLJSONObject connection;
           connection.Add("name", connectionPath.first);
           connection.Add("path", connectionPath.second);
           connection.Add("hidden", false);

           connections.Add(connection);

           m_children.push_back(
                       ObjectPtr(new Folder(parent, connectionPath.first,
                                            connectionPath.second)));
       }
       root.Add("connections", connections);
       doc.Save(m_path);

       CPLDebug("ngstore", "Save connections file to %s", m_path.c_str());
    }

    m_childrenLoaded = true;
    return true;
}

ObjectPtr LocalConnections::getObjectBySystemPath(const std::string &path) const
{
    size_t len = path.size();
    std::string sep = Catalog::separator();
    for(const ObjectPtr &child : m_children) {
        const std::string &testPath = child->path();
        if(len <= testPath.size()) {
            continue;
        }

        if(comparePart(path, testPath, static_cast<unsigned>(testPath.size()))) {
            ObjectContainer * const container =
                    ngsDynamicCast(ObjectContainer, child);
            if(nullptr != container) {
                std::string endPath = path.substr(testPath.size());
#ifdef _WIN32
                std::string from("\\");

                size_t start_pos;
                while((start_pos = endPath.find(from, start_pos)) != std::string::npos) {
                    endPath.replace(start_pos, from.length(), sep);
                    start_pos += sep.length();
                }
#endif
                if(comparePart(endPath, sep, static_cast<unsigned>(sep.size()))) {
                    endPath = endPath.substr(sep.size());
                }
                if(container->loadChildren()) {
                    return container->getObject(endPath);
                }
            }
        }
    }
    return ObjectPtr();
}

}
