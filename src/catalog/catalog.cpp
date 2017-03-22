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

Catalog::Catalog() : ObjectContainer(nullptr, CAT_CONTAINER_ROOT, _("Catalog"))
{
    init();
}

CPLString Catalog::getFullName() const
{
    return CATALOG_PREFIX;
}

ObjectPtr Catalog::getObject(const char *path) const
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

CPLString Catalog::getSeparator()
{
    return "/";
}

unsigned short Catalog::getMaxPathLength()
{
    return 1024;
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
