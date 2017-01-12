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

#ifndef DIRECTORYCONTAINER_H
#define DIRECTORYCONTAINER_H

#include "api.h"

namespace ngs
{
class DirectoryContainer
{
public:
    static bool isEntryDirectory(
            ngsDirectoryContainer* container, int entryIndex)
    {
        return container->entries[entryIndex].type & DET_DIRECTORY;
    }

    static bool isEntryFile(ngsDirectoryContainer* container, int entryIndex)
    {
        return container->entries[entryIndex].type & DET_FILE;
    }

    static char* getPath(ngsDirectoryContainer* container);
    static void freePath(char* path);
    static char* getEntryPath(ngsDirectoryContainer* container, int entryIndex);
    static void freeEntryPath(char* path);

    static bool compareEntries(
            const ngsDirectoryEntry& a, const ngsDirectoryEntry& b);

    static ngsDirectoryContainer* getDirectoryContainer(const char* path);
    static void freeDirectoryContainer(ngsDirectoryContainer* container);

    static void loadDirectoryContainer(const char* path,
            ngsDirectoryContainerLoadCallback callback,
            void* callbackArguments);
};

}  // namespace ngs

#endif  // DIRECTORYCONTAINER_H
