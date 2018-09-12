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
#ifndef NGSFILE_H
#define NGSFILE_H

#include "ngstore/codes.h"
#include "objectcontainer.h"

// gdal
#include "cpl_port.h"

namespace ngs {

class File : public Object
{
public:
    explicit File(ObjectContainer * const parent = nullptr,
         const enum ngsCatalogObjectType type = CAT_FILE_ANY,
         const std::string &name = "",
         const std::string &path = "");

public:
    static bool deleteFile(const std::string &path);
    static time_t modificationDate(const std::string &path);
    static GIntBig fileSize(const std::string &path);
    static bool copyFile(const std::string &src, const std::string &dst,
                         const Progress &progress = Progress());
    static bool moveFile(const std::string &src, const std::string &dst,
                         const Progress &progress = Progress());
    static bool renameFile(const std::string &src, const std::string &dst,
                           const Progress &progress = Progress());
    static bool writeFile(const std::string &file, const void* buffer,
                          size_t size);
    static std::string formFileName(const std::string &path,
                                    const std::string &name,
                                    const std::string &ext = "");
    static std::string resetExtension(const std::string &path,
                                      const std::string &ext = "");
    static std::string getFileName(const std::string &path);
    static std::string getBaseName(const std::string &path);
    static std::string getExtension(const std::string &path);
    static std::string getDirName(const std::string &path);
    static std::string getPath(const std::string &path);
    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
};

}

#endif // NGSFILE_H
