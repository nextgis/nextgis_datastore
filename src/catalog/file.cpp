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

namespace ngs {

constexpr size_t BUFFER_SIZE = 1024 * 8;

File::File(ObjectContainer * const parent,
           const enum ngsCatalogObjectType type,
           const CPLString &name,
           const CPLString &path) :
    Object(parent, type, name, path)
{

}

bool File::deleteFile(const char* path)
{
    int result = VSIUnlink(path);
    if (result == -1)
        return errorMessage(_("Delete file failed! File '%s'"), path);
    return true;
}

time_t File::modificationDate(const char* path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path, &sbuf) == 0 ? sbuf.st_mtime : 0;
}

GIntBig File::fileSize(const char* path)
{
    VSIStatBufL sbuf;
    return VSIStatL(path, &sbuf) == 0 ? sbuf.st_size : 0;
}

bool File::copyFile(const char* src, const char* dst, const Progress& progress)
{
    progress.onProgress(COD_IN_PROCESS, 0.0, _("Start copying %s to %s"),
                        src, dst);
    if(EQUAL(src, dst)) {
        progress.onProgress(COD_FINISHED, 1.0, _("Copied %s to %s"), src, dst);
        return true;
    }

    VSIStatBufL sbuf;
    double totalCount = 1.0;
    if(VSIStatL(src, &sbuf) == 0) {
        totalCount = double(sbuf.st_size) / BUFFER_SIZE;
        if(totalCount < 1.0) {
            totalCount = 1.0;
        }
    }

    VSILFILE* fpOld, * fpNew;
    size_t bytesRead;
    int ret = 0;

    CPLErrorReset();

    // Open old and new file

    fpOld = VSIFOpenL(src, "rb");
    if(fpOld == nullptr) {
        progress.onProgress(COD_COPY_FAILED, 0.0, _("Open input file %s failed"),
                            src);
        return errorMessage(_("Open input file %s failed"), src);
    }

    fpNew = VSIFOpenL(dst, "wb");
    if(fpNew == nullptr) {
        VSIFCloseL(fpOld);
        progress.onProgress(COD_COPY_FAILED, 0.0, _("Open output file %s failed"),
                            dst);
        return errorMessage(_("Open output file %s failed"), dst);
    }

    // Prepare buffer
    GByte* buffer = static_cast<GByte*>(CPLMalloc(BUFFER_SIZE));

    // Copy file over till we run out of stuff
    double counter(0);
    do {
        bytesRead = VSIFReadL(buffer, 1, BUFFER_SIZE, fpOld);
        if(bytesRead == 0) {
            ret = -1;
        }

        if(ret == 0 && VSIFWriteL(buffer, 1, bytesRead, fpNew) < bytesRead) {
            ret = -1;
        }

        counter++;

        if(!progress.onProgress(COD_IN_PROCESS, counter / totalCount,
                               _("Copying %s to %s"), src, dst)) {
            break;
        }

    } while(ret == 0 && bytesRead == BUFFER_SIZE);

    progress.onProgress(COD_FINISHED, 1.0,  _("Copied %s to %s"), src, dst);

    // Cleanup

    VSIFCloseL(fpNew);
    VSIFCloseL(fpOld);

    CPLFree(buffer);

    return ret == 0;
}

bool File::moveFile(const char* src, const char* dst, const Progress& progress)
{
    progress.onProgress(COD_IN_PROCESS, 0.0, _("Start moving %s to %s"),
                        src, dst);
    if(EQUAL(src, dst)) {
        progress.onProgress(COD_FINISHED, 1.0, _("Moved %s to %s"),
                            src, dst);
        return true;
    }

    if(EQUAL(CPLGetPath(dst), CPLGetPath(src))) {
        // If in same directory - make copy
        return renameFile(src, dst, progress);
    }
#ifdef __WINDOWS__
    else if (!EQUALN(dst, "/vsi", 4) && EQUALN(dst, src, 3)) {
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

bool File::renameFile(const char* src, const char* dst, const Progress& progress)
{
    progress.onProgress(COD_IN_PROCESS, 0.0, _("Start rename %s to %s"),
                        src, dst);
    if(VSIRename(src, dst) != 0) {
        progress.onProgress(COD_RENAME_FAILED, 0.0, _("Rename %s to %s failed"),
                            src, dst);
        return errorMessage(_("Rename %s to %s failed"), src, dst);
    }
    progress.onProgress(COD_FINISHED, 0.0, _("Rename %s to %s succeeded"),
                        src, dst);
    return true;
}

bool File::writeFile(const char* file, const void* buffer, size_t size)
{
    VSILFILE* fpNew;
    CPLErrorReset();

    // Open old and new file
    fpNew = VSIFOpenL(file, "wb");
    if(fpNew == nullptr) {
        return errorMessage(_("Open output file %s failed"), file);
    }

    bool result = VSIFWriteL(buffer, 1, size, fpNew) == size;
    // Cleanup

    VSIFCloseL(fpNew);

    return result;
}

bool File::destroy()
{
    if(!File::deleteFile(m_path))
        return false;

    CPLString name = fullName();
    if(m_parent)
        m_parent->notifyChanges();

    Notify::instance().onNotify(name, ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

bool File::canDestroy() const
{
    //return access(m_path, W_OK) != 0;
    VSIStatBufL sbuf;
    return VSIStatL(m_path, &sbuf) == 0 && (sbuf.st_mode & S_IWUSR ||
                                            sbuf.st_mode & S_IWGRP ||
                                            sbuf.st_mode & S_IWOTH);

}

}




