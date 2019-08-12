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
#ifndef NGSARCHIVE_H
#define NGSARCHIVE_H

#include "folder.h"

namespace ngs {

class ArchiveFolder : public Folder
{
public:
    explicit ArchiveFolder(ObjectContainer * const parent = nullptr,
                           const std::string &name = "",
                           const std::string &path = "");
    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;

    // Object interface
public:
    virtual bool canDestroy() const override;

};

class Archive : public ArchiveFolder
{
public:
    explicit Archive(ObjectContainer * const parent = nullptr,
            const enum ngsCatalogObjectType type = CAT_CONTAINER_ARCHIVE,
            const std::string &name = "",
            const std::string &path = "");

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;

    // Static
    static std::string pathPrefix(const enum ngsCatalogObjectType type);

};

}

#endif // NGSARCHIVE_H
