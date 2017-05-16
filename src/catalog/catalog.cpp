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
#include "catalog.h"

// stl
#include <algorithm>

// gdal
#include "cpl_conv.h"
#include "api_priv.h"

#include "folder.h"
#include "localconnections.h"

#include "ngstore/common.h"
#include "util/settings.h"
#include "util/stringutil.h"

// factories
#include "factories/datastorefactory.h"
#include "factories/folderfactory.h"
#include "factories/filefactory.h"
#include "factories/simpledatasetfactory.h"
#include "factories/rasterfactory.h"

namespace ngs {

static CatalogPtr gCatalog;

constexpr const char * CONNECTIONS_DIR = "connections";
constexpr const char * CATALOG_PREFIX = "ngc:/";
constexpr const char * CATALOG_PREFIX_FULL = "ngc://";
constexpr int CATALOG_PREFIX_LEN = length(CATALOG_PREFIX_FULL);

Catalog::Catalog() : ObjectContainer(nullptr, CAT_CONTAINER_ROOT, _("Catalog"))
{
    m_showHidden = Settings::instance().getBool("catalog/show_hidden", true);
}

CPLString Catalog::getFullName() const
{
    return CATALOG_PREFIX;
}

ObjectPtr Catalog::getObject(const char *path)
{
    if(EQUAL(path, CATALOG_PREFIX_FULL))
        return std::static_pointer_cast<Object>(gCatalog);

    // Skip prefix ngc://
    path += CATALOG_PREFIX_LEN;
    return ObjectContainer::getObject(path);
}

ObjectPtr Catalog::getObjectByLocalPath(const char *path)
{
    if(!hasChildren())
        return ObjectPtr();

    // Find LocalConnections
    LocalConnections* localConnections = nullptr;
    for(const ObjectPtr &rootObject : m_children) {
        if(rootObject->getType() == CAT_CONTAINER_LOCALCONNECTION) {
            localConnections = ngsDynamicCast(LocalConnections, rootObject);
            break;
        }
    }

    if(nullptr == localConnections || !localConnections->hasChildren())
        return ObjectPtr();

    return localConnections->getObjectByLocalPath(path);
}

void Catalog::freeResources()
{
    for(ObjectPtr &child : m_children) {
        ObjectContainer * const container = ngsDynamicCast(ObjectContainer,
                                                           child);
        if(nullptr != container) {
            container->clear();
        }
    }
}

void Catalog::createObjects(ObjectPtr object, std::vector<const char *> names)
{
    if(names.empty()) {
        return;
    }

    ObjectContainer* const container = ngsDynamicCast(ObjectContainer, object);
    if(nullptr == container) {
        return;
    }
    // Check each factory for objects
    for(const ObjectFactoryUPtr& factory : m_factories) {
        if(factory->getEnabled()) {
            factory->createObjects(container, &names);
        }
    }
}

bool Catalog::hasChildren()
{
    if(m_childrenLoaded)
        return ObjectContainer::hasChildren();

    const char* settingsPath = CPLGetConfigOption("NGS_SETTINGS_PATH", nullptr);
    if(nullptr == settingsPath)
        return false;

    if(!Folder::isExists(settingsPath)) {
        if(!Folder::mkDir(settingsPath))
            return false;
    }
    // 1. Load factories
    m_factories.push_back(ObjectFactoryUPtr(new DataStoreFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new SimpleDatasetFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new RasterFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new FileFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new FolderFactory()));

    // 2. Load root objects
    const char* connectionsPath = CPLFormFilename(settingsPath, CONNECTIONS_DIR,
                                                  nullptr);
    if(!Folder::isExists(connectionsPath)) {
        if(!Folder::mkDir(connectionsPath))
            return false;
    }
    m_children.push_back(ObjectPtr(new LocalConnections(this, connectionsPath)));

    m_childrenLoaded = true;
    return ObjectContainer::hasChildren();
}

CPLString Catalog::getSeparator()
{
    return "/";
}

unsigned short Catalog::getMaxPathLength()
{
    return 1024;
}

CPLString Catalog::toRelativePath(const Object *object,
                                  const ObjectContainer *objectContainer)
{
    CPLString sep = getSeparator();
    if(nullptr == object || nullptr == objectContainer)
        return "";

    std::vector<ObjectContainer*> parents;
    ObjectContainer* parent = object->getParent();
    if(parent == objectContainer) {
        return "." + sep + object->getName();
    }

    while(parent != nullptr) {
        parents.push_back(parent);
        parent = parent->getParent();
    }

    CPLString prefix("..");
    parent = objectContainer->getParent();
    while(parent != nullptr) {
        auto it = std::find(parents.begin(), parents.end(), parent);
        if(it != parents.end()) {
            --it; // skip common container
            while(it != parents.begin()) {
                prefix += sep + (*it)->getName();
                --it;
            }
            prefix += sep + (*it)->getName();
            return prefix += sep + object->getName();
        }
        prefix += sep + "..";
        parent = parent->getParent();
    }

    return "";
}

ObjectPtr Catalog::fromRelativePath(const char *path,
                                    ObjectContainer *objectContainer)
{
    CPLString sep = getSeparator();
    // Remove separator from begining
    if(EQUALN(sep, path, sep.size()))
        path += sep.size();

    if(EQUALN(".", path, 1) && EQUALN(sep, path + 1, sep.size())) {
        return objectContainer->getObject(path + 1 + sep.size());
    }
    else {
        return objectContainer->getObject(path);
    }
}

#ifdef _WIN32
bool Catalog::isFileHidden(const CPLString &path, const char *name)
#else
bool Catalog::isFileHidden(const CPLString &/*path*/, const char *name)
#endif
{
    if(m_showHidden)
        return false;

    if(EQUALN(name, ".", 1))
        return true;

#ifdef _WIN32
    DWORD attrs = GetFileAttributes(CPLFormFilename(path, name, nullptr));
    if (attrs != INVALID_FILE_ATTRIBUTES)
        return attrs & FILE_ATTRIBUTE_HIDDEN;
#endif
    return false;
}

void Catalog::setShowHidden(bool value)
{
    m_showHidden = value;
    Settings::instance().set("catalog/show_hidden", value);
}

void Catalog::setInstance(Catalog *pointer)
{
    if(gCatalog && nullptr != pointer) // Can be initialized only once.
        return;
    gCatalog.reset(pointer);
}

CatalogPtr Catalog::getInstance()
{
    return gCatalog;
}

}
