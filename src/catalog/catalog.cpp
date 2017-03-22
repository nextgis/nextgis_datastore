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

#include "ngstore/common.h"

namespace ngs {

typedef std::unique_ptr< Catalog > CatalogPtr;
static CatalogPtr gCatalog;

#define CONNECTIONS_DIR "connections"

Catalog::Catalog() : ObjectContainer(nullptr, CAT_CONTAINER_ROOT, _("Catalog"))
{
    init();
}

Catalog::~Catalog()
{

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
