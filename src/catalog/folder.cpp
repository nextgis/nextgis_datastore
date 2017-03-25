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
#include "folder.h"

#include "catalog.h"

namespace ngs {

Folder::Folder(const ObjectContainer *parent,
               const CPLString & name, const CPLString & path) :
    ObjectContainer(parent, CAT_CONTAINER_DIR, name, path)
{

}

bool Folder::hasChildren()
{
    if(m_childrenLoaded)
        return !m_children.empty();

    m_childrenLoaded = true;
    char **items = CPLReadDir(m_path);

    // No children in folder
    if(nullptr == items)
        return false;

    std::vector< const char* > objectNames;
    CatalogPtr catalog = Catalog::getInstance();
    for(int i = 0; items[i] != nullptr; ++i) {
        if(EQUAL(items[i], ".") || EQUAL(items[i], ".."))
            continue;

        if(catalog->isFileHidden(m_path, items[i]))
            continue;

        objectNames.push_back(items[i]);
    }

    catalog->createObjects(m_parent->getChild(m_name), objectNames);

    CSLDestroy(items);

    return !m_children.empty();
}

bool Folder::isExists(const char *path)
{
    VSIStatBuf sbuf;
    return VSIStat(path, &sbuf) == 0;
}

bool Folder::mkDir(const char *path)
{
    return VSIMkdir(path, 0755) == 0;
}

bool Folder::isDir(const char *path)
{
    VSIStatBuf sbuf;
    return VSIStatL(path, &sbuf) == 0 && VSI_ISDIR(sbuf.st_mode);
}

}
