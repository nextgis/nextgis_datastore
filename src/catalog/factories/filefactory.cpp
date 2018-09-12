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
#include "filefactory.h"

#include "catalog/mapfile.h"

namespace ngs {

FileFactory::FileFactory() : ObjectFactory()
{

}

std::string FileFactory::name() const
{
    return _("Files");
}

void FileFactory::createObjects(ObjectContainer * const container,
                                       std::vector<std::string> &names)
{
    auto it = names.begin();
    while( it != names.end() ) {
        std::string ext = File::getExtension(*it);
        if(compare(MapFile::extension(), ext)) {
            std::string path = File::formFileName(container->path(), *it);
            addChild(container, ObjectPtr(new MapFile(container, *it, path)));
            it = names.erase(it);
        } // TODO: Add txt, log support. Do we need prj, spr support here?
        else {
            ++it;
        }
    }
}

}
