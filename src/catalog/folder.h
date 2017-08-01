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
           const CPLString & name = "",
           const CPLString & path = "");
    virtual bool hasChildren() override;

    // Static functions
public:
    static bool isExists(const char* path);
    static bool mkDir(const char* path);
    static bool rmDir(const char* path);
    static bool isDir(const char* path);
    static bool isSymlink(const char* path);
    static bool isHidden(const char* path);

    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual void refresh() override;
    virtual bool isReadOnly() const override;
    virtual int paste(ObjectPtr child, bool move = false,
                      const Options& options = Options(),
                      const Progress& progress = Progress()) override;
    virtual bool canPaste(const enum ngsCatalogObjectType type) const override;
    virtual bool create(const enum ngsCatalogObjectType type,
                        const CPLString& name,
                        const Options& options) override;

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;

protected:
    static CPLString createUniquePath(const CPLString &path,
                                      const CPLString &name,
                                      bool isFolder = true,
                                      const CPLString &add = "",
                                      int counter = 0);

protected:
    std::vector<const char*> fillChildrenNames(char **items);
};

}

#endif // NGSFOLDER_H
