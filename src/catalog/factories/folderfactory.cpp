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
#include "folderfactory.h"

// gdal
#include "cpl_vsi_virtual.h"

#include "catalog/archive.h"
#include "catalog/folder.h"
#include "ngstore/common.h"

namespace ngs {

FolderFactory::FolderFactory() : ObjectFactory()
{
    m_zipSupported = VSIFileManager::GetHandler(
                Archive::getPathPrefix(
                    ngsCatalogObjectType::CAT_CONTAINER_ARCHIVE_ZIP)) != NULL;
}

const char *FolderFactory::getName() const
{
    return _("Folders and archives");
}

void FolderFactory::createObjects(ObjectContainer * const container,
                                       std::vector<const char *> * const names)
{
    std::vector<const char *>::iterator it = names->begin();
    bool deleted;
    while(it != names->end()) {
        deleted = false;
        const char* path = CPLFormFilename(container->getPath(), *it, nullptr);
        if(Folder::isDir(path)) {
            if(container->getType() ==
                                ngsCatalogObjectType::CAT_CONTAINER_ARCHIVE_DIR) { // Check if this is archive folder
                if(m_zipSupported) {
                    CPLString vsiPath = Archive::getPathPrefix(
                                ngsCatalogObjectType::CAT_CONTAINER_ARCHIVE_ZIP);
                    vsiPath += path;
                    addChild(container, ObjectPtr(new ArchiveFolder(container,
                                                                    *it, vsiPath)));
                    it = names->erase(it);
                    deleted = true;
                }
            }
            else {
                addChild(container, ObjectPtr(new Folder(container, *it, path)));
                it = names->erase(it);
                deleted = true;
            }
        }
        else if(m_zipSupported) {
            if(EQUAL(CPLGetExtension(*it),
                     Archive::getExtension(
                         ngsCatalogObjectType::CAT_CONTAINER_ARCHIVE_ZIP))) { // Check if this is archive file
                CPLString vsiPath = Archive::getPathPrefix(
                            ngsCatalogObjectType::CAT_CONTAINER_ARCHIVE_ZIP);
                vsiPath += path;
                addChild(container, ObjectPtr(
                             new Archive(container,
                                         ngsCatalogObjectType::CAT_CONTAINER_ARCHIVE_ZIP,
                                         *it, vsiPath)));
                it = names->erase(it);
                deleted = true;
            }
        }

        if(!deleted) {
            ++it;
        }
    }
}

}
