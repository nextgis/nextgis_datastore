/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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
#include "api_priv.h"

#include "file.h"
#include "folder.h"
#include "localconnections.h"
#include "remoteconnections.h"

#include "ngstore/common.h"
#include "util/settings.h"
#include "util/stringutil.h"

// factories
#include "factories/connectionfactory.h"
#include "factories/datastorefactory.h"
#include "factories/folderfactory.h"
#include "factories/filefactory.h"
#include "factories/simpledatasetfactory.h"
#include "factories/rasterfactory.h"


namespace ngs {

static CatalogPtr gCatalog;

constexpr const char *CONNECTIONS_DIR = "connections";
constexpr const char *CATALOG_PREFIX = "ngc:/";
constexpr const char *CATALOG_PREFIX_FULL = "ngc://";
constexpr int CATALOG_PREFIX_LEN = length(CATALOG_PREFIX_FULL);

Catalog::Catalog() : ObjectContainer(nullptr, CAT_CONTAINER_ROOT, _("Catalog"))
{
    m_showHidden = Settings::instance().getBool("catalog/show_hidden", true);
}

std::string Catalog::fullName() const
{
    return CATALOG_PREFIX;
}

ObjectPtr Catalog::getObject(const std::string &path)
{
    if(compare(path, CATALOG_PREFIX_FULL))
        return std::static_pointer_cast<Object>(gCatalog);

    // Skip prefix ngc://
    return ObjectContainer::getObject(path.substr(CATALOG_PREFIX_LEN));
}

ObjectPtr Catalog::getObjectBySystemPath(const std::string &path)
{
    if(!loadChildren()) {
        return ObjectPtr();
    }

    // Find LocalConnections
    LocalConnections *localConnections = nullptr;
    for(const ObjectPtr &rootObject : m_children) {
        if(rootObject->type() == CAT_CONTAINER_LOCALCONNECTIONS) {
            localConnections = ngsDynamicCast(LocalConnections, rootObject);
            break;
        }
    }

    if(nullptr == localConnections || !localConnections->loadChildren()) {
        return ObjectPtr();
    }

    return localConnections->getObjectBySystemPath(path);
}

bool Catalog::loadChildren()
{
    if(m_childrenLoaded) {
        return true;
    }

    std::string settingsPath = Settings::getConfigOption("NGS_SETTINGS_PATH");
    if(settingsPath.empty()) {
        return false;
    }

    if(!Folder::mkDir(settingsPath, true)) {
        return false;
    }
    // 1. Load factories
    m_factories.push_back(ObjectFactoryUPtr(new ConnectionFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new DataStoreFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new SimpleDatasetFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new RasterFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new FileFactory()));
    m_factories.push_back(ObjectFactoryUPtr(new FolderFactory()));

    // 2. Load root objects
    std::string connectionsPath = File::formFileName(settingsPath, CONNECTIONS_DIR);
    Catalog *parent = const_cast<Catalog*>(this);
    m_children.push_back(ObjectPtr(new LocalConnections(parent, connectionsPath)));
    m_children.push_back(ObjectPtr(new GISServerConnections(parent, connectionsPath)));
    m_children.push_back(ObjectPtr(new DatabaseConnections(parent, connectionsPath)));

    m_childrenLoaded = true;
    return true;
}

void Catalog::freeResources()
{
    for(ObjectPtr &child : m_children) {
        ObjectContainer * const container =
                ngsDynamicCast(ObjectContainer, child);
        if(nullptr != container) {
            container->clear();
        }
    }
}

void Catalog::createObjects(ObjectPtr object, std::vector<std::string> &names)
{
    if(names.empty()) {
        return;
    }

    ObjectContainer * const container = ngsDynamicCast(ObjectContainer, object);
    if(nullptr == container) {
        return;
    }
    // Check each factory for objects
    for(const ObjectFactoryUPtr &factory : m_factories) {
        if(factory->enabled()) {
            factory->createObjects(container, names);
        }
    }
}

std::string Catalog::separator()
{
    return "/";
}

unsigned short Catalog::maxPathLength()
{
    return 1024;
}

std::string Catalog::toRelativePath(const Object *object,
                                    const ObjectContainer *objectContainer)
{
    std::string sep = separator();
    if(nullptr == object || nullptr == objectContainer) {
        return "";
    }

    std::vector<ObjectContainer*> parents;
    ObjectContainer *parent = object->parent();
    if(parent == objectContainer) {
        return "." + sep + object->name();
    }

    while(parent != nullptr) {
        parents.push_back(parent);
        parent = parent->parent();
    }

    std::string prefix("..");
    parent = objectContainer->parent();
    while(parent != nullptr) {
        auto it = std::find(parents.begin(), parents.end(), parent);
        if(it != parents.end()) {
            --it; // skip common container
            while(it != parents.begin()) {
                prefix += sep + (*it)->name();
                --it;
            }
            prefix += sep + (*it)->name();
            return prefix += sep + object->name();
        }
        prefix += sep + "..";
        parent = parent->parent();
    }

    return "";
}

ObjectPtr Catalog::fromRelativePath(const std::string &path,
                                    ObjectContainer *objectContainer)
{
    std::string sep = separator();
    std::string tmp = path;
    // Remove separator from begining
    if(comparePart(sep, path, static_cast<unsigned>(sep.size()))) {
        tmp = tmp.substr(sep.size());
    }

    if(comparePart(".", tmp, 1) &&
            comparePart(sep, tmp.substr(1), static_cast<unsigned>(sep.size()))) {
        return objectContainer->getObject(tmp.substr(1 + sep.size()));
    }
    else {
        return objectContainer->getObject(path);
    }
}

bool Catalog::isFileHidden(const std::string &path, const std::string &name) const

{
    if(m_showHidden)
        return false;

    if(comparePart(name, ".", 1))
        return true;

#ifdef _WIN32
    DWORD attrs = GetFileAttributes(File::formFileName(path, name));
    if (attrs != INVALID_FILE_ATTRIBUTES)
        return attrs & FILE_ATTRIBUTE_HIDDEN;
#else
    ngsUnused(path);
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
    if(gCatalog && nullptr != pointer) { // Can be initialized only once.
        return;
    }
    gCatalog.reset(pointer);
}

CatalogPtr Catalog::instance()
{
    return gCatalog;
}

ObjectPtr Catalog::pointer() const
{
    return Catalog::instance();
}

}
