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
#include "datastorefactory.h"

#include "catalog/file.h"
#include "ds/datastore.h"
#ifndef NGS_MOBILE
#include "ds/mapinfodatastore.h"
#endif // NGS_MOBILE
#include "ds/memstore.h"
#include "ngstore/catalog/filter.h"

namespace ngs {

DataStoreFactory::DataStoreFactory() : ObjectFactory()
{
    m_memSupported = Filter::getGDALDriver(CAT_CONTAINER_MEM);
    m_gpkgSupported = Filter::getGDALDriver(CAT_CONTAINER_NGS);
}


std::string DataStoreFactory::name() const
{
    return _("NextGIS Data and memory store");
}

void DataStoreFactory::createObjects(ObjectContainer * const container,
                                     std::vector<std::string> &names)
{
    auto it = names.begin();
    while( it != names.end() ) {
        std::string ext = File::getExtension(*it);
        if(m_gpkgSupported && compare(ext, DataStore::extension())) {
            std::string path = File::formFileName(container->path(), *it);
            addChild(container, ObjectPtr(new DataStore(container, *it, path)));
            it = names.erase(it);
        }
        else if(m_memSupported && compare(ext, MemoryStore::extension())) {
            std::string path = File::formFileName(container->path(), *it);
            addChild(container, ObjectPtr(new MemoryStore(container, *it, path)));
            it = names.erase(it);
        }
#ifndef NGS_MOBILE
        else if(compare(ext, MapInfoDataStore::extension())) {
            std::string path = File::formFileName(container->path(), *it);
            addChild(container, ObjectPtr(new MapInfoDataStore(container, *it, path)));
            it = names.erase(it);
        }
#endif // NGS_MOBILE
        else if(compare(ext, Dataset::attachmentsFolderExtension())) {
            it = names.erase(it);
        }
        else if(compare(ext, "xml") && (*it).find(DataStore::extension()) !=
                std::string::npos) {
            it = names.erase(it);
        }
        else if(compare(ext, "xml") && (*it).find(MemoryStore::extension()) !=
                std::string::npos) {
            it = names.erase(it);
        }
        else {
            ++it;
        }
    }
}

}
