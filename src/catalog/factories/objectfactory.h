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
#ifndef NGSOBJECTFACTORY_H
#define NGSOBJECTFACTORY_H

#include <vector>
#include <utility>

#include "catalog/objectcontainer.h"

namespace ngs {

typedef std::map<CPLString, std::vector<CPLString>> nameExtMap;
typedef struct _formatExt {
    const char *mainExt;
    const char **mainExts;
    const char **extraExts;
} FORMAT_EXT;

class ObjectFactory
{
public:
    ObjectFactory();
    virtual ~ObjectFactory() = default;
    virtual const char* getName() const = 0;
    virtual void createObjects(ObjectContainer* const container,
                               std::vector<const char *>* const names) = 0;

    bool getEnabled() const;
    void setEnabled(bool enabled);
protected:
    virtual void addChild(ObjectContainer * const container, ObjectPtr object) {
        container->addChild(object);
    }

    typedef struct _formatResult {
        bool isSupported;
        CPLString name;
        std::vector<CPLString> siblingFiles;
    } FORMAT_RESULT;

    static FORMAT_RESULT isFormatSupported(const CPLString& name,
                           std::vector<CPLString> extensions,
                           FORMAT_EXT testExts);
    static void checkAdditionalSiblings(const CPLString& path,
                                        const CPLString& name,
                                        const char **nameAdds,
                                        std::vector<CPLString> &siblingFiles);
    static void eraseNames(const CPLString& name,
                           const std::vector<CPLString> &siblingFiles,
                           std::vector<const char *> * const names);

private:
    bool m_enabled;
};

typedef std::unique_ptr< ObjectFactory > ObjectFactoryUPtr;

}

#endif // NGSOBJECTFACTORY_H
