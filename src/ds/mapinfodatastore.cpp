/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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

#include "mapinfodatastore.h"

#include "catalog/file.h"
#include "catalog/folder.h"
#include "ngstore/catalog/filter.h"

#include "util.h"
#include "util/error.h"

#include "ngstore/version.h"

namespace ngs {

constexpr const char *STORE_EXT = "ngmi"; // NextGIS MapInfo Store
//constexpr int STORE_EXT_LEN = length(STORE_EXT);
constexpr const char *STORE_META_DB = "sys.db";
constexpr const char *HASH_SUFFIX = "hash";

//------------------------------------------------------------------------------
// MapInfoDataStore
//------------------------------------------------------------------------------

MapInfoDataStore::MapInfoDataStore(ObjectContainer * const parent,
                                   const std::string &name,
                                   const std::string &path) :
                  Dataset(parent, CAT_CONTAINER_MAPINFO_STORE, name, path),
                  SpatialDataset()
{
    m_spatialReference = SpatialReferencePtr::importFromEPSG(DEFAULT_EPSG);
}


bool MapInfoDataStore::create(const std::string &path)
{
    resetError();
    if(path.empty()) {
        return errorMessage(_("The path is empty"));
    }

    std::string newPath = path;
    if(!endsWith(path, extension())) {
        newPath += "." + extension();
    }

    if(!Folder::mkDir(newPath, true)) {
        return false;
    }

    GDALDriver *driver = Filter::getGDALDriver(CAT_CONTAINER_MAPINFO_STORE);
    if(driver == nullptr) {
        return errorMessage(_("Driver is not present"));
    }

    std::string dsPath = File::formFileName(newPath, STORE_META_DB);
    Options options;
    options.add("METADATA", "NO");
    options.add("SPATIALITE", "NO");
    options.add("INIT_WITH_EPSG", "NO");

    GDALDataset *DS = driver->Create(dsPath.c_str(), 0, 0, 0, GDT_Unknown,
                                     options.asCPLStringList());
    if(DS == nullptr) {
        return errorMessage(_("Failed to create datastore. %s"), CPLGetLastErrorMsg());
    }

    createMetadataTable(DS);

    GDALClose(DS);
    return true;
}

std::string MapInfoDataStore::extension()
{
    return STORE_EXT;
}

bool MapInfoDataStore::open(unsigned int openFlags, const Options &options)
{
    if(isOpened()) {
        return true;
    }

    std::string dsPath = File::formFileName(m_path, STORE_META_DB);
    bool result = DatasetBase::open(dsPath, openFlags, options);
    if(result) {
        m_addsDS = m_DS;
        m_addsDS->Reference();

        m_metadata = m_addsDS->GetLayerByName(METADATA_TABLE_NAME);
    }
    else {
        return result;
    }

    resetError();

    int version = std::stoi(property(NGS_VERSION_KEY, "0", NG_ADDITIONS_KEY));
    if(version < NGS_VERSION_NUM && !upgrade(version)) {
        return errorMessage(_("Upgrade storage failed"));
    }

    return true;
}

bool MapInfoDataStore::upgrade(int oldVersion)
{
    executeSQL("VACUUM", "SQLite");
    ngsUnused(oldVersion);
    // no structure changes for version 1
    return true;
}

bool MapInfoDataStore::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly()) {
        return false;
    }
    return type == CAT_FC_MAPINFO_TAB;
}


ObjectPtr MapInfoDataStore::create(const enum ngsCatalogObjectType type,
    const std::string &name, const Options& options)
{
    bool overwrite = options.asBool("OVERWRITE", false);
    if (overwrite) {
        ObjectPtr deleteObject = getChild(name);
        if (deleteObject) {
            if(!deleteObject->destroy()) {
                errorMessage(_("Failed overwrite existing object %s. Error: %s"),
                    name.c_str(), CPLGetLastErrorMsg());
                return ObjectPtr();
            }
        }
    }
    std::string newName = normalizeDatasetName(name);

    // Get field definitions
    CREATE_FEATURE_DEFN_RESULT featureDefnStruct =
            createFeatureDefinition(name, options);

    // Create hash table
    createHashTable(m_addsDS, name);

    ObjectPtr object;
    if(type == CAT_FC_MAPINFO_TAB) {
        OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(
                    options.asString("GEOMETRY_TYPE", ""));
        if(wkbUnknown == geomType) {
            errorMessage(_("Unsupported geometry type"));
            return ObjectPtr();
        }

        object = ObjectPtr(createFeatureClass(newName, CAT_FC_MAPINFO_TAB,
                                              featureDefnStruct.defn,
                                              m_spatialReference, geomType,
                                              options));
    }
    else if(type == CAT_TABLE_MAPINFO_TAB) {
        object = ObjectPtr(createTable(newName, CAT_TABLE_MAPINFO_TAB,
                                       featureDefnStruct.defn,
                                       options));
    }

    setMetadata(object, featureDefnStruct.fields, options);

    if(object && m_childrenLoaded) {
        m_children.push_back(object);
    }
    return object;
}


OGRLayer *MapInfoDataStore::createAttachmentsTable(const std::string &name)
{
    if(!m_addsDS) {
        createAdditionsDataset();
    }

    if(!m_addsDS) {
        return nullptr;
    }

    std::string attLayerName(attachmentsTableName(name));
    return ngw::createAttachmentsTable(m_addsDS, attLayerName, m_path);
}

OGRLayer *MapInfoDataStore::createEditHistoryTable(const std::string &name)
{
    if(!m_addsDS) {
        createAdditionsDataset();
    }

    if(!m_addsDS) {
        return nullptr;
    }

    std::string logLayerName(historyTableName(name));
    return ngw::createEditHistoryTable(m_addsDS, logLayerName);
}

static std::string hashTableName(const std::string &name)
{
    return NG_PREFIX + name + "_" + HASH_SUFFIX;
}

OGRLayer *MapInfoDataStore::createHashTable(GDALDataset *ds, const std::string &name)
{
    std::string hashLayerName(hashTableName(name));

    resetError();
    OGRLayer *hashLayer = ds->CreateLayer(hashLayerName.c_str(), nullptr,
                                          wkbNone, nullptr);
    if (nullptr == hashLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Add id field
    OGRFieldDefn fidField(FEATURE_ID_FIELD, OFTInteger64);

    // Add remote id field
    OGRFieldDefn ridField(ngw::REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, ngw::INIT_RID_COUNTER));

    // Add row hash field


    // Create table  fields
    OGRFieldDefn afidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn opField(OPERATION_FIELD, OFTInteger64);
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));
    OGRFieldDefn aridField(ATTACHMENT_REMOTE_ID_KEY, OFTInteger64);
    aridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));

    if(hashLayer->CreateField(&fidField) != OGRERR_NONE ||
       hashLayer->CreateField(&afidField) != OGRERR_NONE ||
       hashLayer->CreateField(&opField) != OGRERR_NONE ||
       hashLayer->CreateField(&ridField) != OGRERR_NONE ||
       hashLayer->CreateField(&aridField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return hashLayer;
}

} // namespace ngs
