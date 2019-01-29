/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019 NextGIS, <info@nextgis.com>
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
#include "api_priv.h"
#include "catalog.h"
#include "file.h"
#include "folder.h"
#include "remoteconnections.h"

#include "factories/connectionfactory.h"
#include "ngstore/catalog/filter.h"
#include "util/notify.h"
#include "util/stringutil.h"



namespace ngs {

constexpr const char *GIS_CONN_DIR = "gisconnections";
constexpr const char *DB_CONN_DIR = "dbconnections";

//------------------------------------------------------------------------------
// Connections
//------------------------------------------------------------------------------
Connections::Connections(ObjectContainer * const parent,
                         const enum ngsCatalogObjectType type,
                         const std::string &name,
                         const std::string &path) :
    ObjectContainer(parent, type, name, path)
{

}

bool Connections::loadChildren()
{
    if(m_childrenLoaded) {
        return true;
    }

    if(m_parent) {
        m_childrenLoaded = true;
        char **items = CPLReadDir(m_path.c_str());

        // No children in folder
        if(nullptr == items) {
            return true;
        }

        std::vector<std::string> objectNames =
                Folder::fillChildrenNames(m_path, items);

        Catalog::instance()->createObjects(m_parent->getChild(m_name),
                                           objectNames);

        CSLDestroy(items);
    }

    return true;
}

bool Connections::canDestroy() const
{
    return false;
}


void Connections::refresh()
{
    if(!m_childrenLoaded) {
        loadChildren();
        return;
    }

    // Current names and new names arrays compare
    if(m_parent) {

        // Fill add names array
        char **items = CPLReadDir(m_path.c_str());

        // No children in folder
        if(nullptr == items) {
            clear();
            return;
        }

        std::vector<std::string> deleteNames, addNames;
        addNames = Folder::fillChildrenNames(m_path, items);

        // Fill delete names array
        for(const ObjectPtr& child : m_children) {
            deleteNames.push_back(child->name());
        }

        // Remove same names from add and delete arrays
        removeDuplicates(deleteNames, addNames);

        // Delete objects
        auto it = m_children.begin();
        while(it != m_children.end()) {
            auto name = (*it)->name();
            auto itdn = std::find(deleteNames.begin(), deleteNames.end(), name);
            if(itdn != deleteNames.end()) {
                deleteNames.erase(itdn);
                it = m_children.erase(it);
            }
            else {
                ++it;
            }
        }

        // Add objects
        Catalog::instance()->createObjects(m_parent->getChild(m_name), addNames);

        CSLDestroy(items);
    }
}

bool Connections::canPaste(const enum ngsCatalogObjectType type) const
{
    return Filter::isConnection(type);
}

int Connections::paste(ObjectPtr child, bool move, const Options& options,
                  const Progress& progress)
{
    std::string newPath;
    if(options.asBool("CREATE_UNIQUE")) {
        newPath = Folder::createUniquePath(m_path, child->name());
    }
    else {
        newPath = File::formFileName(m_path, child->name());
    }

    if(compare(child->path(), newPath)) {
        return COD_SUCCESS;
    }

    File *fileObject = ngsDynamicCast(File, child);
    int result = move ? COD_MOVE_FAILED : COD_COPY_FAILED;
    if(nullptr != fileObject) {
        if(move) {
            result = File::moveFile(child->path(), newPath, progress);
        }
        else {
            result = File::copyFile(child->path(), newPath, progress);
        }

        if(result == COD_SUCCESS) {
            refresh();
        }
    }

    return result;
}

//------------------------------------------------------------------------------
// GISServerConnections
//------------------------------------------------------------------------------
GISServerConnections::GISServerConnections(ObjectContainer * const parent,
                                           const std::string &path) :
    Connections(parent, CAT_CONTAINER_GISCONNECTION,
                    _("GIS Server connections"), path)
{
    m_path = File::formFileName(path, GIS_CONN_DIR);
    Folder::mkDir(m_path, true); // Try to create connections folder
}

bool GISServerConnections::canCreate(const enum ngsCatalogObjectType type) const
{
    switch (type) {
    case CAT_CONTAINER_WFS:
    case CAT_CONTAINER_WMS:
    case CAT_CONTAINER_NGW:
        return true;
    default:
        return false;
    }
}

bool GISServerConnections::create(const enum ngsCatalogObjectType type,
                    const std::string &name,
                    const Options& options)
{
    bool result = false;
    std::string newPath;
    if(options.asBool("CREATE_UNIQUE")) {
        newPath = Folder::createUniquePath(m_path, name);
    }
    else {
        newPath = File::formFileName(m_path, name);
    }
    std::string ext = Filter::extension(type);
    if(!ext.empty()) {
        newPath = File::resetExtension(newPath, ext);
    }
    std::string newName = File::getFileName(newPath);

    switch (type) {
    case CAT_CONTAINER_NGW:
// TODO:
//        result = ConnectionFactory::createRemoteConnection(type, newPath, options);
//        if(result && m_childrenLoaded) {
//            m_children.push_back(ObjectPtr(new Raster(siblingFiles, this, type,
//                                                      newName, newPath)));
//        }
        break;
    default:
        break;
    }

    if(result) {
        std::string nameNotify = fullName() + Catalog::separator() + newName;
        Notify::instance().onNotify(nameNotify, ngsChangeCode::CC_CREATE_OBJECT);
    }

    return result;
}

//------------------------------------------------------------------------------
// DatabaseConnections
//------------------------------------------------------------------------------
DatabaseConnections::DatabaseConnections(ObjectContainer * const parent,
                                           const std::string &path) :
    Connections(parent, CAT_CONTAINER_DBCONNECTION,
                    _("Database connections"), path)
{
    m_path = File::formFileName(path, DB_CONN_DIR);
    Folder::mkDir(m_path, true); // Try to create connections folder
}


bool DatabaseConnections::canCreate(const enum ngsCatalogObjectType type) const
{
    switch (type) {
    case CAT_CONTAINER_POSTGRES:
        return true;
    default:
        return false;
    }
}

bool DatabaseConnections::create(const enum ngsCatalogObjectType type,
                    const std::string &name,
                    const Options& options)
{
    ngsUnused(type);
    ngsUnused(name);
    ngsUnused(options);

    // TODO:
    return false;
}

}
