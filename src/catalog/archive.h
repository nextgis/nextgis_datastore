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
#ifndef NGSARCHIVE_H
#define NGSARCHIVE_H

#include "folder.h"

namespace ngs {

class ArchiveFolder : public Folder
{
public:
    ArchiveFolder(ObjectContainer * const parent = nullptr,
                  const CPLString & name = "",
                  const CPLString & path = "");
    // ObjectContainer interface
public:
    virtual bool canCreate(const ngsCatalogObjectType /*type*/) const override {
        return false;
    }

    // Object interface
public:
    virtual bool canDestroy() const override { return false; }

};

class Archive : public ArchiveFolder
{
public:
    Archive(ObjectContainer * const parent = nullptr,
            const ngsCatalogObjectType type = ngsCatalogObjectType::CAT_CONTAINER_ARCHIVE,
            const CPLString & name = "",
            const CPLString & path = "");

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override { return Folder::canDestroy(); }

    // Static
    static const char* getExtension(const ngsCatalogObjectType type);
    static const char* getPathPrefix(const ngsCatalogObjectType type);

};

}

#endif // NGSARCHIVE_H
