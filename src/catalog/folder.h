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
#ifndef NGSFOLDER_H
#define NGSFOLDER_H

#include "objectcontainer.h"

namespace ngs {

class Folder : public ObjectContainer
{
public:
    explicit Folder(ObjectContainer * const parent = nullptr,
        const std::string &name = "", const std::string &path = "");
    virtual bool loadChildren() override;

    // Static functions
public:
    static std::vector<std::string> listFiles(const std::string &path);
    static bool isExists(const std::string &path);
	static bool isReadOnly(const std::string &path);
    static bool mkDir(const std::string &path, bool recursive = false);
    static bool rmDir(const std::string &path);
    static bool copyDir(const std::string &from, const std::string &to,
        const Progress &progress = Progress());
    static bool moveDir(const std::string &from, const std::string &to,
        const Progress &progress = Progress());
    static bool isDir(const std::string &path);
    static bool isSymlink(const std::string &path);
    static bool isHidden(const std::string &path);
    static std::vector<std::string> fillChildrenNames(const std::string &path,
        char** items);
    static std::string createUniquePath(const std::string &path,
        const std::string &name, bool isFolder = true,
        const std::string &add = "", int counter = 0);
    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual void refresh() override;
    virtual bool isReadOnly() const override;
    virtual int paste(ObjectPtr child, bool move = false,
        const Options& options = Options(),
        const Progress& progress = Progress()) override;
    virtual bool canPaste(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type,
        const std::string &name, const Options &options) override;

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;

protected:
    int pasteFileSource(ObjectPtr child, bool move, const std::string &newPath,
        const Progress &progress);
    int pasteFeatureClass(ObjectPtr child, bool move, const std::string &newPath,
        const Options& options, const Progress& progress);
    int pasteRaster(ObjectPtr child, bool move, const std::string &newPath,
        const Options& options, const Progress& progress);
};

class FolderConnection : public Folder
{
public:
    explicit FolderConnection(ObjectContainer * const parent = nullptr,
        const std::string &name = "", const std::string &path = "");
};

}

#endif // NGSFOLDER_H
