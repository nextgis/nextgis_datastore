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
#include "file.h"

#include "util/error.h"
#include "util/notify.h"

namespace ngs {

File::File(ObjectContainer * const parent,
           const enum ngsCatalogObjectType type,
           const CPLString &name,
           const CPLString &path) :
    Object(parent, type, name, path)
{

}

bool File::deleteFile(const char *path)
{
    int result = VSIUnlink(path);
    if (result == -1)
        return errorMessage(_("Delete file failed! File '%s'"), path);
    return true;
}

bool File::destroy()
{
    if(!File::deleteFile(m_path))
        return false;
    if(m_parent)
        m_parent->notifyChanges();

    Notify::instance().onNotify(getFullName(), ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

bool File::canDestroy() const
{
    return access(m_path, W_OK) == 0;
}

}


