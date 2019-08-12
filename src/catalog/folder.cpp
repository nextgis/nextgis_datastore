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
#include "folder.h"

#include <algorithm>

#include "catalog.h"
#include "file.h"
#include "ds/datastore.h"
#include "ds/memstore.h"
#include "ds/raster.h"
#include "ds/simpledataset.h"
#include "factories/rasterfactory.h"
#include "ngstore/catalog/filter.h"
#include "util/account.h"
#include "util/notify.h"
#include "util/error.h"
#include "archive.h"


namespace ngs {

//------------------------------------------------------------------------------
// FolderConnection
//------------------------------------------------------------------------------
FolderConnection::FolderConnection(ObjectContainer * const parent,
                                   const std::string &name,
                                   const std::string &path) :
    Folder(parent, name, path)
{
    m_type = CAT_CONTAINER_DIR_LINK;
}

//------------------------------------------------------------------------------
// Folder
//------------------------------------------------------------------------------

Folder::Folder(ObjectContainer * const parent,
               const std::string &name,
               const std::string &path) :
    ObjectContainer(parent, CAT_CONTAINER_DIR, name, path)
{

}

std::vector<std::string> Folder::fillChildrenNames(const std::string &path,
                                                   char** items)
{
    std::vector<std::string> names;
    CatalogPtr catalog = Catalog::instance();
    for(int i = 0; items[i] != nullptr; ++i) {
        if(compare(items[i], ".") || compare(items[i], "..")) {
            continue;
        }

        if(catalog->isFileHidden(path, items[i])) {
            continue;
        }

        names.push_back(items[i]);
    }

    return names;
}

bool Folder::loadChildren()
{
    if(m_childrenLoaded) {
        return true;
    }

    if(m_parent) {
        m_childrenLoaded = true;
        char **items = CPLReadDir(m_path.c_str());

        // No children in folder
        if(nullptr == items) {
            return true;
        }

        std::vector<std::string> objectNames = fillChildrenNames(m_path, items);

        Catalog::instance()->createObjects(m_parent->getChild(m_name), objectNames);

        CSLDestroy(items);
    }

    return true;
}

std::vector<std::string> Folder::listFiles(const std::string &path)
{
    std::vector<std::string> out;
    char **items = CPLReadDir(path.c_str());
    if(nullptr != items) {
        int counter = 0;
        while (items[counter] != nullptr) {
            out.push_back(items[counter++]);
        }
        CSLDestroy(items);
    }
    return out;
}

bool Folder::isExists(const std::string &path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path.c_str(), &sbuf) == 0;
}

bool Folder::mkDir(const std::string &path, bool recursive)
{
    if(recursive) {
        if(isExists(path)) {
            return true;
        }

        std::string parentDir = File::getDirName(path);
        if(!mkDir(parentDir, recursive)) {
            return false;
        }
    }

    if(VSIMkdir(path.c_str(), 0755) != 0) {
        return errorMessage(_("Create folder failed! Folder '%s'"), path.c_str());
    }
#ifdef _WIN32
    if (comparePart(File::getFileName(path), ".", 1)) {
        SetFileAttributes(path, FILE_ATTRIBUTE_HIDDEN);
    }
#endif
    return true;

}

bool Folder::rmDir(const std::string &path)
{
    //test if symlink
    if(isSymlink(path)) {
        if(!File::deleteFile(path.c_str())) {
            return false;
        }
    }
    else {
        if (CPLUnlinkTree(path.c_str()) == -1) {
            return errorMessage(_("Delete folder failed! Folder '%s'"),  path.c_str());
        }
    }

    return true;
}

bool Folder::copyDir(const std::string &from, const std::string &to,
                     const Progress& progress)
{
    if(compare(from.c_str(), to.c_str())) {
        return true;
    }

    if(!Folder::isExists(to)) {
        if(!mkDir(to)) {
            return false;
        }
    }

    char **items = CPLReadDir(from.c_str());
    if(nullptr == items) {
        return true;
    }

    int count = CSLCount(items);
    bool result = true;
    for(int i = count - 1; i >= 0; --i ) {
        if(compare(items[i], ".") || compare(items[i], "..")) {
            continue;
        }

        if(progress.onProgress(COD_IN_PROCESS, double(i) / count,
                               _("Copy file %s"), items[i]) == 0) {
            result = false;
            break;
        }

        std::string pathFrom = File::formFileName(from, items[i]);
        std::string pathTo = File::formFileName(to, items[i]);

        if(isDir(pathFrom)) {
            if(!copyDir(pathFrom, pathTo, progress)) {
                result = false;
                break;
            }
        }
        else {
            if(!File::copyFile(pathFrom, pathTo, progress)) {
                result = false;
                break;
            }
        }
    }

    CSLDestroy(items);

    return result;
}

bool Folder::moveDir(const std::string &from, const std::string &to,
                     const Progress& progress)
{
    if(compare(from, to)) {
        return true;
    }

#ifdef __WINDOWS__
    if(startsWith(to, "/vsi") && comparePart(from, to, 3)) {
        return File::renameFile(from, to, progress);
    }
#endif //__WINDOWS__
    bool res = copyDir(from, to, progress);
    if(res) {
        return rmDir(from);
    }

    return false;
}

bool Folder::isDir(const std::string &path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path.c_str(), &sbuf) == 0 && VSI_ISDIR(sbuf.st_mode);
}

bool Folder::isSymlink(const std::string &path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path.c_str(), &sbuf) == 0 && VSI_ISLNK(sbuf.st_mode);
}

bool Folder::isHidden(const std::string &path)
{
#ifdef _WIN32
    DWORD dwAttrs = GetFileAttributes(path);
    if (dwAttrs != INVALID_FILE_ATTRIBUTES)
        return dwAttrs & FILE_ATTRIBUTE_HIDDEN;
#endif
    return comparePart(File::getFileName(path), ".", 1);
}

bool Folder::destroy()
{
    if(!rmDir(m_path)) {
        return false;
    }

    std::string name = fullName();
    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(name, ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

bool Folder::canDestroy() const
{
    return !isReadOnly(); // FIXME: Do we need to check parent can write too?
}

Properties Folder::properties(const std::string &domain) const
{
    if(domain.empty()) {
        Properties out;
        out.add("system_path", m_path);
        return out;
    }
    return ObjectContainer::properties(domain);
}

void Folder::refresh()
{
    if(!m_childrenLoaded) {
        loadChildren();
        return;
    }

    // Current names and new names arrays compare
    if(m_parent) {

        // Fill add names array
        char **items = CPLReadDir(m_path.c_str());

        // No children in folder
        if(nullptr == items) {
            clear();
            return;
        }

        std::vector<std::string> deleteNames, addNames;
        addNames = fillChildrenNames(m_path, items);

        // Fill delete names array
        for(const ObjectPtr& child : m_children) {
            deleteNames.push_back(child->name());
        }

        // Remove same names from add and delete arrays
        removeDuplicates(deleteNames, addNames);

        // Delete objects
        auto it = m_children.begin();
        while(it != m_children.end()) {
            auto name = (*it)->name();
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

int Folder::pasteFileSource(ObjectPtr child, bool move, const std::string &newPath,
                            const Progress &progress)
{
    File *fileObject = ngsDynamicCast(File, child);
    int result = move ? COD_MOVE_FAILED : COD_COPY_FAILED;
    if(nullptr != fileObject) {
        if(move) {
            result = File::moveFile(child->path(), newPath, progress);
        }
        else {
            result = File::copyFile(child->path(), newPath, progress);
        }
    }
    else {
        SimpleDataset *sdts = ngsDynamicCast(SimpleDataset, child);
        if(nullptr != sdts) {
            // Get file list and copy file one by one
            std::vector<std::string> files = sdts->siblingFiles();
            std::string parentPath = sdts->parent()->path();
            for(std::string &file : files) {
                file = parentPath + Catalog::separator() + file;
            }
            files.push_back(child->path());
            unsigned char step = 0;
            Progress newProgress(progress);
            newProgress.setTotalSteps(static_cast<unsigned char>(files.size()));

            std::string srcConstPath = File::resetExtension(child->path());
            srcConstPath.pop_back();
            size_t constPathLen = srcConstPath.length();
            std::string dstConstPath = File::resetExtension(newPath);
            dstConstPath.pop_back();
            for(const auto &file : files) {
                newProgress.setStep(step++);
                std::string newFilePath = dstConstPath + file.substr(constPathLen);
                if(move) {
                    if(!File::moveFile(file, newFilePath, newProgress)) {
                        return COD_MOVE_FAILED;
                    }
                }
                else {
                    if(!File::copyFile(file, newFilePath, newProgress)) {
                        return COD_COPY_FAILED;
                    }
                }
            }
            result = COD_SUCCESS;
        }
    }

    if(result == COD_SUCCESS) {
        refresh();
    }
    return result;
}

int Folder::pasteFeatureClass(ObjectPtr child, bool move,
                              const std::string &newPath,
                              const Options &options, const Progress &progress)
{
    enum ngsCatalogObjectType dstType = static_cast<enum ngsCatalogObjectType>(
                options.asInt("TYPE", 0));

    GDALDriver *driver = Filter::getGDALDriver(dstType);
    if(nullptr == driver || !Filter::isFileBased(dstType)) {
        return outMessage(COD_UNSUPPORTED,
                          _("Destination type %d is not supported"), dstType);
    }

    FeatureClassPtr srcFClass = std::dynamic_pointer_cast<FeatureClass>(child);
    if(!srcFClass) {
        return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                          _("Source object '%s' report type FEATURECLASS, but it is not a feature class"),
                          child->name().c_str());
    }

    // Check function available except GPX.
    if(dstType != CAT_FC_GPX && srcFClass->featureCount() > 1000) {
        const char *appName = CPLGetConfigOption("APP_NAME", "ngstore");
        if(!Account::instance().isFunctionAvailable(appName, "paste_features")) {
            return outMessage(COD_FUNCTION_NOT_AVAILABLE,
                              _("Cannot %s " CPL_FRMT_GIB " features on your plan, or account is not authorized"),
                              move ? _("move") : _("copy"), srcFClass->featureCount());
        }
    }

    std::string newName = File::getBaseName(newPath);

    bool toMulti = options.asBool("FORCE_GEOMETRY_TO_MULTI", false);
    OGRFeatureDefn * const srcDefinition = srcFClass->definition();
    std::vector<OGRwkbGeometryType> geometryTypes =
            srcFClass->geometryTypes();
    OGRwkbGeometryType filterFeometryType =
            FeatureClass::geometryTypeFromName(
                options.asString("ACCEPT_GEOMETRY", "ANY"));
    for(OGRwkbGeometryType geometryType : geometryTypes) {
        if(filterFeometryType != geometryType &&
                filterFeometryType != wkbUnknown) {
            continue;
        }
        std::string createName = newName;
        OGRwkbGeometryType newGeometryType = geometryType;
        if(geometryTypes.size () > 1 && filterFeometryType == wkbUnknown) {
            createName += "_";
            createName += FeatureClass::geometryTypeName(geometryType,
                                    FeatureClass::GeometryReportType::SIMPLE);
            if(toMulti && geometryType < wkbMultiPoint) {
                newGeometryType =
                        static_cast<OGRwkbGeometryType>(geometryType + 3);
            }
        }

        std::unique_ptr<Dataset> ds(Dataset::create(this, dstType, createName, options));
        if(!ds || !ds->isOpened()) {
            return outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        }

        std::string fc_name = options.asString("LAYER_NAME", newName);
        if(dstType == CAT_FC_GPX) {
            newGeometryType = wkbPoint25D;
        }
        std::unique_ptr<FeatureClass> dstFClass(ds->createFeatureClass(fc_name,
            dstType, srcDefinition, srcFClass->spatialReference(),
            newGeometryType, options));
        if(nullptr == dstFClass) {
            return move ? COD_MOVE_FAILED : COD_COPY_FAILED;
        }

        // Create fields map. We expected equal count of fields
        FieldMapPtr fieldMap(srcFClass->fields(), dstFClass->fields());
        int result = dstFClass->copyFeatures(srcFClass, fieldMap,
                                             filterFeometryType,
                                             progress, options);
        if(result != COD_SUCCESS) {
            return result;
        }
    }

    refresh();
    return COD_SUCCESS;
}

int Folder::paste(ObjectPtr child, bool move, const Options &options,
                  const Progress &progress)
{
    std::string fileName = options.asString("DS_NAME", child->name());
    std::string newPath;
    if(options.asBool("CREATE_UNIQUE", false)) {
        newPath = createUniquePath(m_path, fileName);
    }
    else {
        newPath = File::formFileName(m_path, fileName);
    }

    if(compare(child->path(), newPath)) {
        return COD_SUCCESS;
    }

    if(Filter::isLocalDir(child->type())) {
        return copyDir(child->path(), newPath, progress) ? COD_SUCCESS : COD_COPY_FAILED;
    }
    else if(Filter::isFileBased(child->type())) {
        return pasteFileSource(child, move, newPath, progress);
    }
    else if(Filter::isFeatureClass(child->type())) {
        return pasteFeatureClass(child, move, newPath, options, progress);
    }

    return move ? COD_MOVE_FAILED : COD_COPY_FAILED;
}

bool Folder::canPaste(const enum ngsCatalogObjectType type) const
{
    if(isReadOnly())
        return false;
    return Filter::isFileBased(type) || Filter::isFeatureClass(type);
}

bool Folder::isReadOnly() const
{
    //  Is is working on Windows?
    return access(m_path.c_str(), W_OK) != 0;
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
    case CAT_CONTAINER_MEM:
    case CAT_CONTAINER_ARCHIVE_ZIP:
        return true;
    default:
        return false;
    }
}

bool Folder::create(const enum ngsCatalogObjectType type,
                    const std::string &name,
                    const Options& options)
{
    if(!loadChildren()) {
        return false;
    }
    bool result = false;
    std::string newName = name;
    // Add extension if not present
    if(!compare(File::getExtension(name), Filter::extension(type))) {
        newName = name + "." + Filter::extension(type);
    }

    if(options.asBool("CREATE_UNIQUE")) {
        newName = createUniqueName(newName, false);
    }

    std::string newPath = File::formFileName(m_path, newName);
    if(hasChild(newName)) {
        if(options.asBool("OVERWRITE")) {
            if(!File::deleteFile(newPath)) {
                return errorMessage(_("Failed to overwrite %s"), newName.c_str());
            }
        }
        else {
            return errorMessage(_("Object %s already exists. Add overwrite option or create_unique option to create object here"),
                              newName.c_str());
        }
    }

    std::vector<std::string> siblingFiles;

    switch (type) {
    case CAT_CONTAINER_DIR:
        result = mkDir(newPath);
        if(result) {
            m_children.push_back(ObjectPtr(new Folder(this, newName, newPath)));
        }
        break;
    case CAT_CONTAINER_NGS:
        result = DataStore::create(newPath);
        if(result) {
            m_children.push_back(ObjectPtr(new DataStore(this, newName, newPath)));
        }
        break;
    case CAT_RASTER_TMS:
        result = RasterFactory::createRemoteConnection(type, newPath, options);
        if(result) {
            m_children.push_back(ObjectPtr(new Raster(siblingFiles, this, type,
                                                      newName, newPath)));
        }
        break;
    case CAT_CONTAINER_MEM:
        result = MemoryStore::create(newPath, options);
        if(result) {
            m_children.push_back(ObjectPtr(new MemoryStore(this, newName, newPath)));
        }
        break;
    case CAT_CONTAINER_ARCHIVE_ZIP:
        {
            void *archive = CPLCreateZip(newPath.c_str(), nullptr);
            if(archive == nullptr) {
                return errorMessage(_("Failed to create %s."), newName.c_str());
            }

            if(CPLCloseZip(archive) != CE_None) {
                return errorMessage(_("Failed to create %s."), newName.c_str());
            }
            m_children.push_back(ObjectPtr(new Archive(this, CAT_CONTAINER_ARCHIVE_ZIP, newName, newPath)));
            result = true;
        }
        break;
    default:
        break;
    }

    if(result) {
        std::string nameNotify = fullName() + Catalog::separator() + newName;
        Notify::instance().onNotify(nameNotify, ngsChangeCode::CC_CREATE_OBJECT);
    }

    return result;
}

std::string Folder::createUniquePath(const std::string &path,
                                     const std::string &name,
                                     bool isFolder, const std::string &add,
                                     int counter)
{
    std::string resultPath;
    if(counter > 0) {
        CPLString newAdd;
        newAdd.Printf("%s(%d)", add.c_str(), counter);
        std::string tmpName = File::getBaseName(name) + newAdd;
        if(isFolder) {
            resultPath = File::formFileName(path, tmpName);
        }
        else {
            resultPath = File::formFileName(path, tmpName, File::getExtension(name));
        }
    }
    else {
        resultPath = File::formFileName(path, name);
    }

    if(isExists(resultPath)) {
        return createUniquePath(path, name, isFolder, add, counter + 1);
    }
    else {
        return resultPath;
    }
}

}
