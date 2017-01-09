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

#include "catalogobjectcontainer.h"

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

namespace ngs
{
// static
char* CatalogObjectContainer::getPath(ngsCatalogObjectContainer* container)
{
    const char* tmpPath = CPLFormFilename(
            container->parentPath, container->directoryName, nullptr);
    char* dirPath = new char[strlen(tmpPath) + 1];
    strcpy(dirPath, tmpPath);
    return dirPath;
}

// static
void CatalogObjectContainer::freePath(char* path)
{
    delete[] path;
}

// static
char* CatalogObjectContainer::getEntryPath(
        ngsCatalogObjectContainer* container, int entryIndex)
{
    char* dirPath = getPath(container);

    ngsCatalogObject* entry = &container->entries[entryIndex];
    const char* tmpPath =
            CPLFormFilename(dirPath, entry->baseName, entry->extension);
    char* entryPath = new char[strlen(tmpPath) + 1];
    strcpy(entryPath, tmpPath);

    freePath(dirPath);
    return entryPath;
}

// static
void CatalogObjectContainer::freeEntryPath(char* path)
{
    delete[] path;
}

// static
bool CatalogObjectContainer::compareEntries(
        const ngsCatalogObject& a, const ngsCatalogObject& b)
{
    if ((a.type & COT_DIRECTORY) && !(b.type & COT_DIRECTORY)) {
        return true;
    }

    if (!(a.type & COT_DIRECTORY) && (b.type & COT_DIRECTORY)) {
        return false;
    }

    int res = strcmp(a.baseName, b.baseName);
    return res < 0 ? true : false;
}

// static
ngsCatalogObjectContainer* CatalogObjectContainer::getDirectoryContainer(
        const char* path)
{
    if (!path) {
        return nullptr;
    }

    const char* tmpPath = CPLCleanTrailingSlash(path);
    char cleanedPath[strlen(tmpPath) + 1];
    strcpy(cleanedPath, tmpPath);

    char** pList = VSIReadDir(cleanedPath);
    if (!pList) {
        return nullptr;
    }

    int nameCount = 0;
    for (char** pFullName = pList; *pFullName != nullptr; ++pFullName) {
        ++nameCount;
    }

    const char* dirName = CPLGetFilename(cleanedPath);
    const char* parentPath = CPLGetPath(cleanedPath);

    ngsCatalogObjectContainer* pContainer = new ngsCatalogObjectContainer;
    pContainer->directoryName = new char[strlen(dirName) + 1];
    strcpy(pContainer->directoryName, dirName);

    if (!strcmp(parentPath, cleanedPath) || !strcmp(parentPath, "")) {
        pContainer->parentPath = "";
    } else {
        pContainer->parentPath = new char[strlen(parentPath) + 1];
        strcpy(pContainer->parentPath, parentPath);
    }

    std::vector<ngsCatalogObject> tmpEntries;
    tmpEntries.reserve(nameCount);

    for (char** pFullName = pList; *pFullName != nullptr; ++pFullName) {
        if (!strcmp(*pFullName, "..") || !strcmp(*pFullName, ".")) {
            continue;
        }

        const char* baseName = CPLGetBasename(*pFullName);
        const char* extension = CPLGetExtension(*pFullName);
        const char* fullPath =
                CPLFormFilename(cleanedPath, baseName, extension);

        VSIStatBufL stat;
        if (VSIStatL(fullPath, &stat) == -1) {
            return nullptr;  // TODO: throw error
        }

        enum ngsCatalogObjectType  type;
        if (VSI_ISDIR(stat.st_mode)) {
            type = COT_DIRECTORY;
        } else if (VSI_ISREG(stat.st_mode)) {
            type = COT_FILE;
        } else {
            type = COT_UNKNOWN;
        }

        ngsCatalogObject entry;
        entry.type = type;
        entry.baseName = new char[strlen(baseName) + 1];
        strcpy(entry.baseName, baseName);
        entry.extension = new char[strlen(extension) + 1];
        strcpy(entry.extension, extension);

        tmpEntries.push_back(entry);
    }

    CSLDestroy(pList);

    pContainer->entryCount = tmpEntries.size();
    pContainer->entries = new ngsCatalogObject[tmpEntries.size()];
    std::sort(tmpEntries.begin(), tmpEntries.end(), compareEntries);
    std::copy(tmpEntries.begin(), tmpEntries.end(), pContainer->entries);

    return pContainer;
}

// static
void CatalogObjectContainer::freeDirectoryContainer (
        ngsCatalogObjectContainer* container)
{
    for (int i = 0; i < container->entryCount; ++i) {
        ngsCatalogObject* entry = &container->entries[i];
        delete[] entry->baseName;
        delete[] entry->extension;
    }

    delete[] container->entries;
    delete[] container->directoryName;
    delete container;
}

typedef struct _ContainerArgs
{
    char* path;
    ngsDirectoryContainerLoadCallback callback;
    void* callbackArguments;
} ContainerArgs;

void loadDirectoryContainerThread(void* arguments)
{
#ifdef _DEBUG
    std::chrono::high_resolution_clock::time_point t1 =
            std::chrono::high_resolution_clock::now();
    std::cout << "Start loadDirectoryContainerThread"
              << "\n";
#endif  //DEBUG

    if (!arguments) {
        return;
    }
    ContainerArgs* args = static_cast<ContainerArgs*>(arguments);
    ngsCatalogObjectContainer* container =
            CatalogObjectContainer::getDirectoryContainer(args->path);
    if (!container) {
        return; // TODO: error message
    }

    if (nullptr != args->callback) {
        args->callback(container, args->callbackArguments);
    }

    CatalogObjectContainer::freeDirectoryContainer(container);
    delete args->path;
    delete args;

#ifdef _DEBUG
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::high_resolution_clock::now() - t1)
                            .count();
    std::cout << "Finish loadDirectoryContainerThread at " << duration << " ms"
              << "\n";
#endif  //DEBUG
}

// static
void CatalogObjectContainer::loadDirectoryContainer(const char* path,
        ngsDirectoryContainerLoadCallback callback,
        void* callbackArguments)
{
    //CPLLockHolder tilesHolder(m_hThreadLock);
    ContainerArgs* args = new ContainerArgs;
    args->callback = callback;
    args->callbackArguments = callbackArguments;
    args->path = new char[strlen(path) + 1];
    strcpy(args->path, path);

    CPLJoinableThread* loadThread =
            CPLCreateJoinableThread(loadDirectoryContainerThread, args);
}

}  // namespace ngs
