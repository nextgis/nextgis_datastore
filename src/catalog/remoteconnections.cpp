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
#include "ngw.h"
#include "remoteconnections.h"

#include "factories/connectionfactory.h"
#include "ngstore/catalog/filter.h"
#include "util/account.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/stringutil.h"

#include <algorithm>

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
        CPLStringList items(CPLReadDir(m_path.c_str()));

        // No children in folder
        if(items.empty()) {
            return true;
        }

        std::vector<std::string> objectNames =
                Folder::fillChildrenNames(m_path, items);

        Catalog::instance()->createObjects(m_parent->getChild(m_name),
                                           objectNames);

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
        CPLStringList items(CPLReadDir(m_path.c_str()));

        // No children in folder
        if(items.empty()) {
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
    }
}

bool Connections::canPaste(const enum ngsCatalogObjectType type) const
{
    return Filter::isConnection(type);
}

int Connections::paste(ObjectPtr child, bool move, const Options& options,
                  const Progress& progress)
{
    std::string newName = child->name();
    if(options.asBool("CREATE_UNIQUE")) {
        newName = createUniqueName(child->name(), false);
    }

    std::string newPath = File::formFileName(m_path, newName);
    if(hasChild(newName)) {
        if(options.asBool("OVERWRITE")) {
            if(!File::deleteFile(newPath)) {
                return outMessage(COD_DELETE_FAILED, _("Failed to overwrite %s"),
                                  newName.c_str());
            }
        }
        else {
            return outMessage(COD_CANCELED, _("Object %s already exists. Add overwrite option or create_unique option to create object here"),
                              newName.c_str());
        }
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
    Connections(parent, CAT_CONTAINER_GISCONNECTIONS,
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
        return true;
    case CAT_CONTAINER_NGW:
    {
        int counter = 0;
        for (const auto &child : m_children) {
            if(child->type() == type) {
                counter++;
            }

            if(counter > 1) {
                break;
            }
        }

        if(counter > 1) {
            const char *appName = CPLGetConfigOption("APP_NAME", "ngstore");
            if(!Account::instance().isFunctionAvailable(appName, "create_ngw_connection")) {
                return errorMessage(_("Cannot create more than 1 NextGIS Web connection on your plan, or account is not authorized"));
            }
        }
        return true;
    }
    default:
        return false;
    }
}

ObjectPtr GISServerConnections::create(const enum ngsCatalogObjectType type,
                    const std::string &name,
                    const Options& options)
{
    if(!loadChildren()) {
        return ObjectPtr();
    }
    std::string newName = name;
    if(!compare(File::getExtension(name), Filter::extension(type))) {
        newName = name + "." + Filter::extension(type);
    }

    if(options.asBool("CREATE_UNIQUE", false)) {
        newName = createUniqueName(newName, false);
    }

    std::string newPath = File::formFileName(m_path, newName);
    ObjectPtr child = getChild(newName);
    if(child) {
        if(options.asBool("OVERWRITE", false)) {
            if(!child->destroy()) {
                errorMessage(_("Failed to overwrite %s"), newName.c_str());
                return ObjectPtr();
            }
        }
        else {
            errorMessage(_("Object %s already exists. Add overwrite option or create_unique option to create object here"),
                newName.c_str());
            return ObjectPtr();
        }
    }
    child = ObjectPtr();
    switch (type) {
    case CAT_CONTAINER_NGW:
        if(ConnectionFactory::createRemoteConnection(type, newPath, options)) {
            child = ObjectPtr(new NGWConnection(this, newName, newPath));
            m_children.push_back(child);
        }
        break;
    default:
        break;
    }

    if(child) {
        std::string nameNotify = fullName() + Catalog::separator() + newName;
        Notify::instance().onNotify(nameNotify, ngsChangeCode::CC_CREATE_OBJECT);
    }

    return child;
}

//------------------------------------------------------------------------------
// DatabaseConnections
//------------------------------------------------------------------------------
DatabaseConnections::DatabaseConnections(ObjectContainer * const parent,
                                           const std::string &path) :
    Connections(parent, CAT_CONTAINER_DBCONNECTIONS,
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

ObjectPtr DatabaseConnections::create(const enum ngsCatalogObjectType type,
                    const std::string &name, const Options& options)
{
    ngsUnused(type);
    ngsUnused(name);
    ngsUnused(options);

    // TODO:
    return ObjectPtr();
}

}
