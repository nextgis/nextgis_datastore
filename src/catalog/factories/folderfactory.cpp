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

#include "catalog/folder.h"
#include "ngstore/common.h"

namespace ngs {

FolderFactory::FolderFactory()
{

}

const char *ngs::FolderFactory::getName() const
{
    return _("Folders");
}

void ngs::FolderFactory::createObjects(ObjectContainer * const container,
                                       std::vector<const char *> * const names)
{
    std::vector<const char *>::iterator it = names->begin();
    while( it != names->end() ) {
        const char* path = CPLFormFilename(container->getPath(), *it, nullptr);
        if(Folder::isDir(path)) {
            container->addObject(ObjectPtr(new Folder(container, *it, path)));
            it = names->erase(it);
        }
        else {
            ++it;
        }
    }
}

}
