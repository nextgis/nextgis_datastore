/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-20120 NextGIS, <info@nextgis.com>
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
#ifndef NGSOBJECTFACTORY_H
#define NGSOBJECTFACTORY_H

#include <vector>
#include <utility>

#include "catalog/objectcontainer.h"

namespace ngs {

constexpr const char *KEY_TYPE = "type";

typedef std::map<std::string, std::vector<std::string>> nameExtMap;
typedef struct _formatExt {
    std::string mainExt;
    std::vector<std::string> mainExts;
    std::vector<std::string> extraExts;
} FORMAT_EXT;

class ObjectFactory
{
public:
    ObjectFactory();
    virtual ~ObjectFactory() = default;
    virtual std::string name() const = 0;
    virtual void createObjects(ObjectContainer* const container,
                               std::vector<std::string> &names) = 0;

    bool enabled() const;
    void setEnabled(bool enabled);
protected:
    virtual void addChild(ObjectContainer * const container, ObjectPtr object);

    typedef struct _formatResult {
        bool isSupported;
        std::string name;
        std::vector<std::string> siblingFiles;
    } FORMAT_RESULT;

    static FORMAT_RESULT isFormatSupported(const std::string &name,
                           std::vector<std::string> extensions,
                           FORMAT_EXT testExts);
    static void checkAdditionalSiblings(const std::string &path,
                                        const std::string &name,
                                        const std::vector<std::string> &nameAdds,
                                        std::vector<std::string> &siblingFiles);
    static void eraseNames(const std::string &name,
                           const std::vector<std::string> &siblingFiles,
                           std::vector<std::string> &names);

private:
    bool m_enabled;
};

using ObjectFactoryUPtr = std::unique_ptr<ObjectFactory>;
enum ngsCatalogObjectType typeFromConnectionFile(const std::string &path);

}

#endif // NGSOBJECTFACTORY_H
