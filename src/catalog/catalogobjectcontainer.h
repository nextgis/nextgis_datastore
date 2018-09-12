/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
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

#ifndef CATALOGOBJECTCONTAINER_H
#define CATALOGOBJECTCONTAINER_H

#include "api.h"

#include "cpl_string.h"

#include <memory>

namespace ngs
{
class CatalogObjectContainer;
using CatalogObjectContainerPtr = std::shared_ptr<CatalogObjectContainer>;

class CatalogObjectContainer
{
public:
    static bool isEntryDirectory(
            ngsCatalogObjectContainer *container, int entryIndex)
    {
        return container->entries[entryIndex].type & COT_DIRECTORY;
    }

    static bool isEntryFile(
            ngsCatalogObjectContainer *container, int entryIndex)
    {
        return container->entries[entryIndex].type & COT_FILE;
    }

    static char* getPath(ngsCatalogObjectContainer* container);
    static void freePath(char* path);
    static char* getEntryPath(
            ngsCatalogObjectContainer* container, int entryIndex);
    static void freeEntryPath(char* path);

    static bool compareEntries(
            const ngsCatalogObject& a, const ngsCatalogObject& b);

    static ngsCatalogObjectContainer* getDirectoryContainer(const char* path);
    static void freeDirectoryContainer(ngsCatalogObjectContainer* container);

    static void loadDirectoryContainer(const char* path,
            ngsDirectoryContainerLoadCallback callback,
            void* callbackArguments);


    static CatalogObjectContainerPtr load(const char* path);

    std::string path() const {
        return m_Path;
    }

    ngsCatalogObject getCatalogObject(int id);

protected:
    std::string m_Path;
};

}  // namespace ngs

#endif  // CATALOGOBJECTCONTAINER_H
