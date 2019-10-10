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
#include <ngstore/catalog/filter.h>
#include "folderfactory.h"

// gdal
#include "cpl_vsi_virtual.h"

#include "catalog/archive.h"
#include "catalog/file.h"
#include "catalog/folder.h"
#include "ngstore/common.h"
#include "util/stringutil.h"

namespace ngs {

FolderFactory::FolderFactory() : ObjectFactory()
{
    m_zipSupported = VSIFileManager::GetHandler(
                Archive::pathPrefix(CAT_CONTAINER_ARCHIVE_ZIP).c_str()) != nullptr;
}

std::string FolderFactory::name() const
{
    return _("Folders and archives");
}

void FolderFactory::createObjects(ObjectContainer * const container,
                                       std::vector<std::string> &names)
{
    auto it = names.begin();
    while(it != names.end()) {
        std::string path = File::formFileName(container->path(), *it);
        if(Folder::isDir(path)) {
            if(container->type() == CAT_CONTAINER_ARCHIVE_DIR) { // Check if this is archive folder
                if(m_zipSupported) {
                    std::string vsiPath = Archive::pathPrefix(
                                CAT_CONTAINER_ARCHIVE_ZIP);
                    vsiPath += path;
                    addChild(container,
                             ObjectPtr(new ArchiveFolder(container, *it, vsiPath)));
                    it = names.erase(it);
                    continue;
                }
            }
            else {
                addChild(container, ObjectPtr(new Folder(container, *it, path)));
                it = names.erase(it);
                continue;
            }
        }
        else if(m_zipSupported) {
            if(compare(File::getExtension(*it),
                       Filter::extension(CAT_CONTAINER_ARCHIVE_ZIP))) { // Check if this is archive file
                addChild(container,
                         ObjectPtr(new Archive(container,
                                               CAT_CONTAINER_ARCHIVE_ZIP,
                                               *it, path)));
                it = names.erase(it);
                continue;
            }
        }
        ++it;
    }
}

}
