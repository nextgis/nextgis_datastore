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
#include "folder.h"

#include <algorithm>

#include "catalog.h"
#include "file.h"
#include "ds/datastore.h"
#include "ds/raster.h"
#include "ds/simpledataset.h"
#include "factories/rasterfactory.h"
#include "ngstore/catalog/filter.h"
#include "util/notify.h"
#include "util/error.h"

namespace ngs {

Folder::Folder(ObjectContainer * const parent,
               const CPLString & name,
               const CPLString & path) :
    ObjectContainer(parent, CAT_CONTAINER_DIR, name, path)
{

}

bool Folder::hasChildren()
{
    if(m_childrenLoaded)
        return !m_children.empty();

    if(m_parent) {
        m_childrenLoaded = true;
        char** items = CPLReadDir(m_path);

        // No children in folder
        if(nullptr == items)
            return false;

        std::vector< const char*> objectNames = fillChildrenNames(items);

        Catalog::instance()->createObjects(m_parent->getChild(m_name), objectNames);

        CSLDestroy(items);
    }

    return !m_children.empty();
}

bool Folder::isExists(const char* path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path, &sbuf) == 0;
}

bool Folder::mkDir(const char* path)
{
    if( VSIMkdir( path, 0755 ) != 0 )
        return errorMessage(_("Create folder failed! Folder '%s'"), path);
#ifdef _WIN32
    if (EQUALN(CPLGetFilename(path), ".", 1)) {
        SetFileAttributes(path, FILE_ATTRIBUTE_HIDDEN);
    }
#endif
    return true;

}

bool Folder::isDir(const char* path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path, &sbuf) == 0 && VSI_ISDIR(sbuf.st_mode);
}

bool Folder::isSymlink(const char *path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path, &sbuf) == 0 && VSI_ISLNK(sbuf.st_mode);
}

bool Folder::isHidden(const char *path)
{
#ifdef _WIN32
    DWORD dwAttrs = GetFileAttributes(path);
    if (dwAttrs != INVALID_FILE_ATTRIBUTES)
        return dwAttrs & FILE_ATTRIBUTE_HIDDEN;
#endif
    return EQUALN(CPLGetFilename(path), ".", 1);
}

bool Folder::destroy()
{
    //test if symlink
    if(isSymlink(m_path)) {
        if(!File::deleteFile(m_path)) {
            return false;
        }
    }
    else {
        if (CPLUnlinkTree(m_path) == -1) {
            return errorMessage(_("Delete folder failed! Folder '%s'"), 
                                m_path.c_str());
        }
    }
    
    CPLString name = fullName();
    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(name, ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

bool Folder::canDestroy() const
{
    return !isReadOnly(); // FIXME: Do we need to check parent too?
}

void Folder::refresh()
{
    if(!m_childrenLoaded) {
        hasChildren();
        return;
    }

    // Current names and new names arrays compare
    if(m_parent) {

        // Fill add names array
        char** items = CPLReadDir(m_path);

        // No children in folder
        if(nullptr == items) {
            clear();
            return;
        }

        std::vector<const char*> deleteNames, addNames;
        addNames = fillChildrenNames(items);

        // Fill delete names array
        for(const ObjectPtr& child : m_children)
            deleteNames.push_back(child->name());

        // Remove same names from add and delete arrays
        removeDuplicates(deleteNames, addNames);

        // Delete objects
        auto it = m_children.begin();
        while(it != m_children.end()) {
            const char* name = (*it)->name();
            auto itdn = std::find(deleteNames.begin(), deleteNames.end(), name);
            if(itdn != deleteNames.end()) {
                deleteNames.erase(itdn);
                it = m_children.erase(it);
            }
            else {
                ++it;
            }
        }

        // Add objects
        Catalog::instance()->createObjects(m_parent->getChild(m_name), addNames);

        CSLDestroy(items);
    }
}

int Folder::paste(ObjectPtr child, bool move, const Options& options,
                  const Progress& progress)
{
    CPLString newPath;
    if(options.boolOption("CREATE_UNIQUE")) {
        newPath = createUniquePath(m_path, child->name());
    }
    else {
        newPath = CPLFormFilename(m_path, child->name(), nullptr);
    }

    if(EQUAL(child->path(), newPath)) {
        return ngsCode::COD_SUCCESS;
    }

    File* file = ngsDynamicCast(File, child);
    if(nullptr != file) {
        if(move) {
            return File::moveFile(child->path(), newPath, progress);
        }
        else {
            return File::copyFile(child->path(), newPath, progress);
        }
    }

    // TODO: Add support for FeatureClass/Table from SimpleDataset

    SimpleDataset* sdts = ngsDynamicCast(SimpleDataset, child);
    if(nullptr != sdts) {
        // Get file list and copy file one by one
        std::vector<CPLString> files = sdts->siblingFiles();
        CPLString parentPath = sdts->parent()->path();
        for(CPLString& file : files) {
            file = parentPath + Catalog::separator() + file;
        }
        files.push_back(child->path());
        unsigned char step = 0;
        Progress newProgress(progress);
        newProgress.setTotalSteps(static_cast<unsigned char>(files.size()));

        CPLString srcConstPath = CPLResetExtension(child->path(), "");
        srcConstPath.pop_back();
        size_t constPathLen = srcConstPath.length();
        CPLString dstConstPath = CPLResetExtension(newPath, "");
        dstConstPath.pop_back();
        for(auto file : files) {
            newProgress.setStep(step++);
            CPLString newFilePath = dstConstPath + file.substr(constPathLen);
            if(move) {
                if(!File::moveFile(file, newFilePath, newProgress)) {
                    return ngsCode::COD_MOVE_FAILED;
                }
            }
            else {
                if(!File::copyFile(file, newFilePath, newProgress)) {
                    return ngsCode::COD_MOVE_FAILED;
                }
            }
        }
        return ngsCode::COD_SUCCESS;
    }

    return move ? ngsCode::COD_MOVE_FAILED : ngsCode::COD_COPY_FAILED;
}

bool Folder::canPaste(const enum ngsCatalogObjectType type) const
{
    if(isReadOnly())
        return false;
    return Filter::isFileBased(type);
}

bool Folder::isReadOnly() const
{
    //  Is is working on Windows?
    return access(m_path, W_OK) != 0;
//    VSIStatBufL sbuf;
//    return VSIStatL(m_path, &sbuf) == 0 && (sbuf.st_mode & S_IWUSR ||
//                                            sbuf.st_mode & S_IWGRP ||
//                                            sbuf.st_mode & S_IWOTH);
}

bool Folder::canCreate(const enum ngsCatalogObjectType type) const
{
    switch (type) {
    case CAT_CONTAINER_DIR:
    case CAT_CONTAINER_NGS:
    case CAT_RASTER_TMS:
        return true;
    default:
        return false;
    }
}

bool Folder::create(const enum ngsCatalogObjectType type, const CPLString &name,
                         const Options & options)
{
    bool result = false;
    CPLString newPath;
    if(options.boolOption("CREATE_UNIQUE")) {
        newPath = createUniquePath(m_path, name);
    }
    else {
        newPath = CPLFormFilename(m_path, name, nullptr);
    }
    const char* ext = Filter::getExtension(type);
    if(nullptr != ext && !EQUAL(ext, "")) {
        newPath = CPLResetExtension(newPath, ext);
    }
    CPLString newName = CPLGetFilename(newPath);
    std::vector<CPLString> siblingFiles;

    switch (type) {
    case CAT_CONTAINER_DIR:
        result = mkDir(newPath);
        if(result && m_childrenLoaded) {
            m_children.push_back(ObjectPtr(new Folder(this, newName, newPath)));
        }
        break;
    case CAT_CONTAINER_NGS:
        result = DataStore::create(newPath);
        if(result && m_childrenLoaded) {
            m_children.push_back(ObjectPtr(new DataStore(this, newName, newPath)));
        }
        break;
    case CAT_RASTER_TMS:
        result = RasterFactory::createRemoteConnection(type, newPath, options);
        if(result && m_childrenLoaded) {
            m_children.push_back(ObjectPtr(new Raster(siblingFiles, this, type,
                                                      newName, newPath)));
        }
        break;
    default:
        break;
    }

    if(result) {
        CPLString name = fullName() + Catalog::separator() + newName;
        Notify::instance().onNotify(name, ngsChangeCode::CC_CREATE_OBJECT);
    }

    return result;
}

CPLString Folder::createUniquePath(const CPLString &path, const CPLString &name,
                                   bool isFolder, const CPLString &add,
                                   int counter)
{
    CPLString resultPath;
    if(counter > 0) {
        CPLString newAdd;
        newAdd.Printf("%s(%d)", add.c_str(), counter);
        CPLString tmpName = CPLGetBasename(name) + newAdd;
        if(isFolder) {
            resultPath = CPLFormFilename(path, tmpName, nullptr);
        }
        else {
            resultPath = CPLFormFilename(path, tmpName, CPLGetExtension(name));
        }
    }
    else {
        resultPath = CPLFormFilename(path, name, nullptr);
    }

    if(isExists(resultPath))
        return createUniquePath(path, name, isFolder, add, counter + 1);
    else
        return resultPath;
}

std::vector<const char*> Folder::fillChildrenNames(char** items)
{
    std::vector<const char*> names;
    CatalogPtr catalog = Catalog::instance();
    for(int i = 0; items[i] != nullptr; ++i) {
        if(EQUAL(items[i], ".") || EQUAL(items[i], ".."))
            continue;

        if(catalog->isFileHidden(m_path, items[i]))
            continue;

        names.push_back(items[i]);
    }

    return names;
}


}

