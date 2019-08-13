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
#include "file.h"

#include "util/error.h"
#include "util/notify.h"
#include "util/stringutil.h"

//gdal
#include "cpl_conv.h"

namespace ngs {

constexpr size_t BUFFER_SIZE = 1024 * 8;

File::File(ObjectContainer * const parent,
           const enum ngsCatalogObjectType type,
           const std::string &name,
           const std::string &path) :
    Object(parent, type, name, path)
{

}

bool File::deleteFile(const std::string &path)
{
    int result = VSIUnlink(path.c_str());
    if (result == -1)
        return errorMessage(_("Delete file failed! File '%s'"), path.c_str());
    return true;
}

time_t File::modificationDate(const std::string &path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path.c_str(), &sbuf) == 0 ? sbuf.st_mtime : 0;
}

GIntBig File::fileSize(const std::string &path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path.c_str(), &sbuf) == 0 ?
                static_cast<size_t>(sbuf.st_size) : 0;
}

bool File::copyFile(const std::string &src, const std::string &dst, const Progress& progress)
{
    progress.onProgress(COD_IN_PROCESS, 0.0, _("Start copying %s to %s"),
                        src.c_str(), dst.c_str());
    if(compare(src, dst)) {
        progress.onProgress(COD_FINISHED, 1.0, _("Copied %s to %s"),
                            src.c_str(), dst.c_str());
        return true;
    }

    VSIStatBufL sbuf;
    double totalCount = 1.0;
    if(VSIStatL(src.c_str(), &sbuf) == 0) {
        totalCount = double(sbuf.st_size) / BUFFER_SIZE;
        if(totalCount < 1.0) {
            totalCount = 1.0;
        }
    }

    VSILFILE* fpOld, * fpNew;
    bool ret = true;
    resetError();

    // Open old and new file

    fpOld = VSIFOpenL(src.c_str(), "rb");
    if(fpOld == nullptr) {
        progress.onProgress(COD_COPY_FAILED, 0.0, _("Open input file %s failed"),
                            src.c_str());
        return errorMessage(_("Open input file %s failed"), src.c_str());
    }

    fpNew = VSIFOpenL(dst.c_str(), "wb");
    if(fpNew == nullptr) {
        VSIFCloseL(fpOld);
        const char *err = CPLGetLastErrorMsg();
        progress.onProgress(COD_COPY_FAILED, 0.0, _("Open output file %s failed. Error: %s"),
                            dst.c_str(), err);
        return errorMessage(_("Open output file %s failed"), dst.c_str());
    }

    // Prepare buffer
    GByte* buffer = static_cast<GByte*>(CPLMalloc(BUFFER_SIZE));

    // Copy file over till we run out of stuff
    double counter(0);
    while(true) {
        size_t read = VSIFReadL(buffer, 1, BUFFER_SIZE, fpOld);
        size_t written = VSIFWriteL(buffer, 1, read, fpNew);
        if(written != read) {
            errorMessage("Copying of %s to %s failed", src.c_str(), dst.c_str());
            ret = false;
            break;
        }

        counter++;

        if(!progress.onProgress(COD_IN_PROCESS, counter / totalCount,
                               _("Copying %s to %s"), src.c_str(), dst.c_str())) {
            ret = false;
            break;
        }
        if(read < BUFFER_SIZE) {
            break;
        }
    }

    progress.onProgress(COD_FINISHED, 1.0,
                        _("Copied %s to %s"), src.c_str(), dst.c_str());

    // Cleanup

    VSIFCloseL(fpNew);
    VSIFCloseL(fpOld);

    CPLFree(buffer);

    return ret;
}

bool File::moveFile(const std::string &src, const std::string &dst, const Progress& progress)
{
    progress.onProgress(COD_IN_PROCESS, 0.0, _("Start moving %s to %s"),
                        src.c_str(), dst.c_str());
    if(compare(src, dst)) {
        progress.onProgress(COD_FINISHED, 1.0, _("Moved %s to %s"),
                            src.c_str(), dst.c_str());
        return true;
    }

    if(EQUAL(CPLGetPath(dst.c_str()), CPLGetPath(src.c_str()))) {
        // If in same directory - make copy
        return renameFile(src, dst, progress);
    }
#ifdef __WINDOWS__
    else if (!comparePart(dst, "/vsi", 4) && comparePart(dst, src, 3)) {
        // If in same disc - copy/rename
        return renameFile(src, dst, progress);
    }
#endif // TODO: UNIX renameFile add support too
    else {
        // If in different discs - copy/move
        bool res = copyFile(src, dst, progress);
        if(res) {
            res = deleteFile(src);
        }
        return res;
    }
    //return false;
}

bool File::renameFile(const std::string &src, const std::string &dst, const Progress& progress)
{
    progress.onProgress(COD_IN_PROCESS, 0.0, _("Start rename %s to %s"),
                        src.c_str(), dst.c_str());
    if(VSIRename(src.c_str(), dst.c_str()) != 0) {
        progress.onProgress(COD_RENAME_FAILED, 0.0, _("Rename %s to %s failed"),
                            src.c_str(), dst.c_str());
        return errorMessage(_("Rename %s to %s failed"), src.c_str(), dst.c_str());
    }
    progress.onProgress(COD_FINISHED, 0.0, _("Rename %s to %s succeeded"),
                        src.c_str(), dst.c_str());
    return true;
}

bool File::writeFile(const std::string &file, const void* buffer, size_t size)
{
    VSILFILE* fpNew;
    CPLErrorReset();

    // Open old and new file
    fpNew = VSIFOpenL(file.c_str(), "wb");
    if(fpNew == nullptr) {
        return errorMessage(_("Open output file %s failed"), file.c_str());
    }

    bool result = VSIFWriteL(buffer, 1, size, fpNew) == size;
    // Cleanup

    VSIFCloseL(fpNew);

    return result;
}

std::string File::formFileName(const std::string &path,
                               const std::string &name,
                               const std::string &ext)
{
    return CPLFormFilename(path.c_str(), name.c_str(), ext.c_str());
}

std::string File::resetExtension(const std::string &path,
                                 const std::string &ext)
{
    return CPLResetExtension(path.c_str(), ext.c_str());
}

std::string File::getFileName(const std::string &path)
{
    return CPLGetFilename(path.c_str());
}

std::string File::getBaseName(const std::string &path)
{
    return CPLGetBasename(path.c_str());
}

std::string File::getExtension(const std::string &path)
{
    return CPLGetExtension(path.c_str());
}

std::string File::getDirName(const std::string &path)
{
    return CPLGetDirname(path.c_str());
}

std::string File::getPath(const std::string &path)
{
    return CPLGetPath(path.c_str());
}

bool File::destroy()
{
    if(!File::deleteFile(m_path))
        return false;

    std::string name = fullName();
    if(m_parent)
        m_parent->notifyChanges();

    Notify::instance().onNotify(name, ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

bool File::canDestroy() const
{
    //return access(m_path, W_OK) != 0;
    VSIStatBufL sbuf;
    return VSIStatL(m_path.c_str(), &sbuf) == 0 && (sbuf.st_mode & S_IWUSR ||
                                                    sbuf.st_mode & S_IWGRP ||
                                                    sbuf.st_mode & S_IWOTH);

}

}




