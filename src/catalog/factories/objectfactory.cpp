/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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
#include "objectfactory.h"

#include <algorithm>

// gdal
#include "cpl_json.h"
#include "cpl_port.h"

#include "catalog/file.h"
#include "catalog/folder.h"
#include "util/stringutil.h"

namespace ngs {

ObjectFactory::ObjectFactory() : m_enabled(true)
{

}

bool ObjectFactory::enabled() const
{
    return m_enabled;
}

void ObjectFactory::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void ObjectFactory::addChild(ObjectContainer * const container, ObjectPtr object)
{
    container->addChild(object);
}

ObjectFactory::FORMAT_RESULT ObjectFactory::isFormatSupported(
        const std::string &name, std::vector<std::string> extensions,
        FORMAT_EXT testExts)
{
    ObjectFactory::FORMAT_RESULT out;
    out.isSupported = false;

    unsigned char counter = 0;

    for(const auto& extension : extensions) {
        if(compare(extension, testExts.mainExt)) { // Check main format extension
            counter++;
            out.name = File::formFileName("", name, extension);
        }
        else {
            bool breakCompare = false;
            for(const std::string &mainExt : testExts.mainExts) {
                // Check additional format extensions [required]
                if(compare(extension, mainExt)) {
                    counter++;
                    breakCompare = true;
                    out.siblingFiles.push_back(
                                File::formFileName("", name, extension));
                }
            }
            if(!breakCompare) {
                for(const std::string &extraExt : testExts.extraExts) {
                    // Check additional format extensions [optional]
                    if(compare(extension, extraExt)) {
                        out.siblingFiles.push_back(
                                    File::formFileName("", name, extension));
                    }
                }
            }
        }
    }

    if(counter > testExts.mainExts.size() ) {
        out.isSupported = true;
    }

    return out;
}

void ObjectFactory::checkAdditionalSiblings(const std::string &path,
                                            const std::string &name,
                                            const std::vector<std::string> &nameAdds,
                                            std::vector<std::string> &siblingFiles)
{
    for(const std::string &nameAdd : nameAdds) {
        std::string newName = name + nameAdd;
        if(Folder::isExists(File::formFileName(path, newName))) {
            siblingFiles.push_back(newName);
        }
    }
}

void ObjectFactory::eraseNames(const std::string &name,
                               const std::vector<std::string> &siblingFiles,
                               std::vector<std::string> &names)
{
    auto lastItem = names.end();
    for(const auto& siblingFile : siblingFiles) {
        lastItem = std::remove(names.begin(), lastItem, siblingFile);
    }
    lastItem = std::remove(names.begin(), lastItem, name);

    if(lastItem != names.end()) {
        names.erase(lastItem, names.end());
    }
}

enum ngsCatalogObjectType typeFromConnectionFile(const std::string &path)
{
    CPLJSONDocument connectionFile;
    if(connectionFile.Load(path)) {
        return static_cast<enum ngsCatalogObjectType>(
                    connectionFile.GetRoot().GetInteger(
                        KEY_TYPE, CAT_UNKNOWN));
    }
    return CAT_UNKNOWN;
}

}
