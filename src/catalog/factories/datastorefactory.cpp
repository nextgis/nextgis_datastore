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
#include "datastorefactory.h"

#include "ds/datastore.h"

namespace ngs {

DataStoreFactory::DataStoreFactory() : ObjectFactory()
{

}


const char *DataStoreFactory::getName() const
{
    return _("NextGIS DataStore");
}

void DataStoreFactory::createObjects(ObjectContainer * const container,
                                     std::vector<const char *> * const names)
{
    std::vector<const char *>::iterator it = names->begin();
    while( it != names->end() ) {
        const char* ext = CPLGetExtension(*it);
        if(EQUAL(ext, DataStore::extension())) {
            const char* path = CPLFormFilename(container->path(), *it, nullptr);
            addChild(container, ObjectPtr(new DataStore(container, *it, path)));
            it = names->erase(it);
        }
        else if(EQUAL(ext, Dataset::attachmentsFolderExtension())) {
            it = names->erase(it);
        }
        else if(EQUAL(ext, "xml") && strstr(*it, DataStore::extension()) !=
                nullptr) {
            it = names->erase(it);
        }
        else {
            ++it;
        }
    }
}

}
