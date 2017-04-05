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
#include "mapfile.h"

namespace ngs {

MapFile::MapFile(ObjectContainer * const parent,
                 const CPLString &name,
                 const CPLString &path) :
    File(parent, ngsCatalogObjectType::CAT_FILE_NGMAPDOCUMENT, name, path)
{

}

bool MapFile::open()
{

}

bool MapFile::save(MapViewPtr mapView)
{
    if(nullptr != m_parent) {
        m_parent->notifyChanges();


    }

    Notify::instance().onNotify(path, ngsChangeCodes::CC_CREATE_OBJECT);
}

bool MapFile::destroy()
{
    if(m_mapView)
        m_mapView->close();
    return File::destroy();
}

}

