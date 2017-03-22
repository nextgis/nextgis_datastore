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

#include "cpl_conv.h"

#include "api_priv.h"
#include "localconnections.h"
#include "ngstore/common.h"

namespace ngs {

typedef std::unique_ptr< Catalog > CatalogPtr;
static CatalogPtr gCatalog;

int constexpr length(const char* str)
{
    return *str ? 1 + length(str + 1) : 0;
}
constexpr const char * CONNECTIONS_DIR = "connections";
constexpr const char * CATALOG_PREFIX = "ngs://";
constexpr int CATALOG_PREFIX_LEN = length(CATALOG_PREFIX);

Catalog::Catalog() : ObjectContainer(nullptr, CAT_CONTAINER_ROOT, _("Catalog")),
    showHidden(true)
{
    init();
}

CPLString Catalog::getFullName() const
{
    return CATALOG_PREFIX;
}

ObjectPtr Catalog::getObject(const char *path)
{
    // Skip prefix ngs://
    path += CATALOG_PREFIX_LEN;
    return ObjectContainer::getObject(path);
}

void Catalog::freeResources()
{
    for(ObjectPtr &child : children) {
        ObjectContainer * const container = ngsDynamicCast(ObjectContainer,
                                                           child);
        if(nullptr != container) {
            container->clear();
        }
    }
}

void Catalog::createObjects(ObjectPtr object, std::vector<const char *> names)
{
    if(names.empty())
        return;
    ObjectContainer* const container = ngsDynamicCast(ObjectContainer, object);
    if(nullptr == container)
        return;
    //TODO: Check each factory for objects
}

CPLString Catalog::getSeparator()
{
    return "/";
}

unsigned short Catalog::getMaxPathLength()
{
    return 1024;
}

#ifdef _WIN32
bool Catalog::isFileHidden(const CPLString &filePath, const char *fileName)
#else
bool Catalog::isFileHidden(const CPLString &/*filePath*/, const char *fileName)
#endif
{
    if(showHidden)
        return false;

    if(EQUALN(fileName, ".", 1))
        return true;

#ifdef _WIN32
    DWORD attrs = GetFileAttributes(CPLFormFilename(filePath, fileName, NULL));
    if (attrs != INVALID_FILE_ATTRIBUTES)
        return attrs & FILE_ATTRIBUTE_HIDDEN;
#endif
    return false;
}

void Catalog::init()
{
    const char* settingsPath = CPLGetConfigOption("NGS_SETTINGS_PATH", nullptr);
    if(nullptr == settingsPath)
        return;
    // 1. Load factories

    // 2. Load root objects
    const char* connectionsPath = CPLFormFilename(settingsPath, CONNECTIONS_DIR,
                                                  "");
    children.push_back(ObjectPtr(new LocalConnections(this, connectionsPath)));
}

void Catalog::setShowHidden(bool value)
{
    showHidden = value;
}

void Catalog::setInstance(Catalog *pointer)
{
    if(gCatalog && nullptr != pointer) // Can be initialized only once.
        return;
    gCatalog.reset(pointer);
}

Catalog* Catalog::getInstance()
{
    return gCatalog.get();
}

}
