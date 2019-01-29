/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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

#include "datastore.h"

// gdal
#include "cpl_conv.h"
#include "cpl_vsi.h"

// stl
#include <iostream>

#include "api_priv.h"
#include "geometry.h"
#include "storefeatureclass.h"

#include "catalog/catalog.h"
#include "catalog/folder.h"

#include "ngstore/catalog/filter.h"
#include "ngstore/codes.h"
#include "ngstore/util/constants.h"
#include "ngstore/version.h"

#include "util/notify.h"
#include "util/error.h"
#include "util/stringutil.h"

#include <catalog/file.h>

namespace ngs {

constexpr const char *STORE_EXT = "ngst"; // NextGIS Store
constexpr int STORE_EXT_LEN = length(STORE_EXT);

//------------------------------------------------------------------------------
// DataStore
//------------------------------------------------------------------------------

DataStore::DataStore(ObjectContainer * const parent,
                     const std::string &name,
                     const std::string &path) :
    Dataset(parent, CAT_CONTAINER_NGS, name, path),
    m_disableJournalCounter(0)
{
    m_spatialReference = new OGRSpatialReference;
    m_spatialReference->importFromEPSG(DEFAULT_EPSG);
}

DataStore::~DataStore()
{
    m_spatialReference->Release();
    m_spatialReference = nullptr;
}

bool DataStore::isNameValid(const std::string &name) const
{
    if(name.empty())
        return false;
    if(comparePart(name, STORE_EXT, STORE_EXT_LEN))
        return false;

    return Dataset::isNameValid(name);
}

std::string DataStore::normalizeFieldName(const std::string &name) const
{
    if(compare(REMOTE_ID_KEY, name)) {
        std::string out = name + "_";
        return out;
    }
    if(compare("fid", name) || compare("geom", name)) {
        std::string out = name + "_";
        return out;
    }
    return Dataset::normalizeFieldName(name);
}

void DataStore::fillFeatureClasses() const
{
    for(int i = 0; i < m_DS->GetLayerCount(); ++i){
        OGRLayer *layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            if(skipFillFeatureClass(layer)) {
                continue;
            }

            const char *layerName = layer->GetName();
            DataStore *parent = const_cast<DataStore*>(this);
            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new StoreTable(layer, parent,
                                                              layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new StoreFeatureClass(layer, parent,
                                                                     layerName)));
            }
        }
    }
}

bool DataStore::create(const std::string &path)
{
    CPLErrorReset();
    if(path.empty()) {
        return errorMessage(_("The path is empty"));
    }

    GDALDriver *poDriver = Filter::getGDALDriver(CAT_CONTAINER_NGS);
    if(poDriver == nullptr) {
        return errorMessage(_("GeoPackage driver is not present"));
    }

    GDALDataset *DS = poDriver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    if(DS == nullptr) {
        return errorMessage(CPLGetLastErrorMsg());
    }

    createMetadataTable(DS);

    GDALClose(DS);
    return true;
}

std::string DataStore::extension()
{
    return STORE_EXT;
}

bool DataStore::open(unsigned int openFlags, const Options &options)
{
    if(isOpened()) {
        return true;
    }

    if(!Dataset::open(openFlags, options))
        return false;

    CPLErrorReset();

    int version = std::stoi(property(NGS_VERSION_KEY, "0"));

    if(version < NGS_VERSION_NUM && !upgrade(version)) {
        return errorMessage(_("Upgrade storage failed"));
    }

    return true;
}

FeatureClass *DataStore::createFeatureClass(const std::string &name,
                                            enum ngsCatalogObjectType objectType,
                                            OGRFeatureDefn * const definition,
                                            OGRSpatialReference *spatialRef,
                                            OGRwkbGeometryType type,
                                            const Options &options,
                                            const Progress& progress)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened"));
        return nullptr;
    }

    ngsUnused(objectType);

    MutexHolder holder(m_executeSQLMutex);

    OGRLayer *layer = m_DS->CreateLayer(name.c_str(), spatialRef, type,
                                        options.asCPLStringList());

    if(layer == nullptr) {
        errorMessage(CPLGetLastErrorMsg());
        return nullptr;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        std::string newFieldName;
        if(definition->GetFieldCount() - 1 == i) {
            newFieldName = srcField->GetNameRef();
        }
        else {
            newFieldName = normalizeFieldName(srcField->GetNameRef());
            if(!compare(newFieldName, srcField->GetNameRef())) {
                progress.onProgress(COD_WARNING, 0.0,
                                    _("Field %s of source table was renamed to %s in destination tables"),
                                    srcField->GetNameRef(), newFieldName.c_str());
            }
        }

        dstField.SetName(newFieldName.c_str());
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(CPLGetLastErrorMsg());
            return nullptr;
        }
    }

    FeatureClass *out = new StoreFeatureClass(layer, this, name);

    if(options.asBool("CREATE_OVERVIEWS", false) &&
            !options.asString("ZOOM_LEVELS", "").empty()) {
        out->createOverviews(progress, options);
    }

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->fullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

Table *DataStore::createTable(const std::string &name,
                              enum ngsCatalogObjectType objectType,
                              OGRFeatureDefn* const definition,
                              const Options& options, const Progress& progress)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened"));
        return nullptr;
    }

    ngsUnused(objectType);

    MutexHolder holder(m_executeSQLMutex);

    OGRLayer *layer = m_DS->CreateLayer(name.c_str(), nullptr, wkbNone,
                                        options.asCPLStringList());

    if(layer == nullptr) {
        errorMessage(CPLGetLastErrorMsg());
        return nullptr;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        std::string newFieldName;
        if(definition->GetFieldCount() - 1 == i) {
            newFieldName = srcField->GetNameRef();
        }
        else {
            newFieldName = normalizeFieldName(srcField->GetNameRef());
            if(!compare(newFieldName, srcField->GetNameRef())) {
                progress.onProgress(COD_WARNING, 0.0,
                                    _("Field %s of source table was renamed to %s in destination tables"),
                                    srcField->GetNameRef(), newFieldName.c_str());
            }
        }

        dstField.SetName(newFieldName.c_str());
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(CPLGetLastErrorMsg());
            return nullptr;
        }
    }

    Table *out = new StoreTable(layer, this, name);

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->fullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

bool DataStore::setProperty(const std::string &key, const std::string &value,
                            const std::string &domain)
{
    MutexHolder holder(m_executeSQLMutex);
    return m_DS->SetMetadataItem(key.c_str(), value.c_str(), domain.c_str()) ==
            OGRERR_NONE;
}

std::string DataStore::property(const std::string &key,
                                const std::string &defaultValue,
                                const std::string &domain) const
{
    MutexHolder holder(m_executeSQLMutex);
    const char *out = m_DS->GetMetadataItem(key.c_str(), domain.c_str());
    return nullptr == out ? defaultValue : out;
}

Properties DataStore::properties(const std::string &domain) const
{
    if(nullptr == m_DS) {
        return Properties();
    }

    return Properties(m_DS->GetMetadata(domain.c_str()));
}

bool DataStore::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly())
        return false;
    return type == CAT_FC_GPKG || type == CAT_TABLE_GPKG;

}

bool DataStore::create(const enum ngsCatalogObjectType type,
                       const std::string &name, const Options& options)
{
    std::string newName = normalizeDatasetName(name);

    // Get field definitions
    OGRFeatureDefn fieldDefinition(newName.c_str());
    int fieldCount = options.asInt("FIELD_COUNT", 0);
    struct fieldData {
        std::string name, alias;
    };
    std::vector<fieldData> fields;

    for(int i = 0; i < fieldCount; ++i) {
        std::string fieldName = options.asString(CPLSPrintf("FIELD_%d_NAME", i), "");
        if(fieldName.empty()) {
            return errorMessage(_("Name for field %d is not defined"), i);
        }

        std::string fieldAlias = options.asString(CPLSPrintf("FIELD_%d_ALIAS", i), "");
        if(fieldAlias.empty()) {
            fieldAlias = fieldName;
        }
        fieldData data = { fieldName, fieldAlias };
        fields.push_back(data);

        OGRFieldType fieldType = FeatureClass::fieldTypeFromName(
                    options.asString(CPLSPrintf("FIELD_%d_TYPE", i), ""));
        OGRFieldDefn field(fieldName.c_str(), fieldType);
        std::string defaultValue = options.asString(
                    CPLSPrintf("FIELD_%d_DEFAULT_VAL", i), "");
        if(!defaultValue.empty()) {
            field.SetDefault(defaultValue.c_str());
        }
        fieldDefinition.AddFieldDefn(&field);
    }

    // Add remote id field
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));
    fieldDefinition.AddFieldDefn(&ridField);

    ObjectPtr object;
    if(type == CAT_FC_GPKG) {
        OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(
                    options.asString("GEOMETRY_TYPE", ""));
        if(wkbUnknown == geomType) {
            return errorMessage(_("Unsupported geometry type"));
        }

        object = ObjectPtr(createFeatureClass(newName, CAT_FC_GPKG,
                                              &fieldDefinition,
                                              m_spatialReference, geomType,
                                              options));
    }
    else if(type == CAT_TABLE_GPKG) {
        object = ObjectPtr(createTable(newName, CAT_TABLE_GPKG, &fieldDefinition,
                                       options));
    }

    if(!object) {
        return false;
    }

    Table *table = ngsDynamicCast(Table, object);

    // Store aliases and field original names in properties
    for(size_t i = 0; i < fields.size(); ++i) {
        table->setProperty("FIELD_" + std::to_string(i) + "_NAME", fields[i].name, NG_ADDITIONS_KEY);
        table->setProperty("FIELD_" + std::to_string(i) + "_ALIAS", fields[i].alias, NG_ADDITIONS_KEY);
    }

    bool saveEditHistory = options.asBool(LOG_EDIT_HISTORY_KEY, false);
    table->setProperty(LOG_EDIT_HISTORY_KEY, saveEditHistory ? "ON" : "OFF", NG_ADDITIONS_KEY);

    // Store user defined options in properties
    for(auto it = options.begin(); it != options.end(); ++it) {
        if(comparePart(it->first, USER_PREFIX_KEY, USER_PREFIX_KEY_LEN)) {
            table->setProperty(it->first.substr(USER_PREFIX_KEY_LEN),
                               it->second, USER_KEY);
        }
    }

    if(m_childrenLoaded) {
        m_children.push_back(object);
    }

    return true;
}

bool DataStore::upgrade(int oldVersion)
{
    ngsUnused(oldVersion);
    // no structure changes for version 1
    return true;
}

void DataStore::enableJournal(bool enable)
{
    if(enable) {        
        m_disableJournalCounter--;
        if(m_disableJournalCounter == 0) {
            // executeSQL("PRAGMA synchronous = FULL", "SQLITE");
            executeSQL("PRAGMA journal_mode = DELETE", "SQLITE");
            //executeSQL("PRAGMA count_changes=ON", "SQLITE"); // This pragma is deprecated
        }
    }
    else {
        CPLAssert(m_disableJournalCounter < 255); // only 255 layers can simultanious load geodata
        m_disableJournalCounter++;
        if(m_disableJournalCounter == 1) {
            // executeSQL("PRAGMA synchronous = OFF", "SQLITE");
            executeSQL("PRAGMA journal_mode = OFF", "SQLITE");
            //executeSQL("PRAGMA count_changes=OFF", "SQLITE"); // This pragma is deprecated
            // executeSQL ("PRAGMA locking_mode=EXCLUSIVE", "SQLITE");
            // executeSQL ("PRAGMA cache_size=15000", "SQLITE");
        }
    }
}

OGRLayer *DataStore::createAttachmentsTable(const std::string &name)
{
    if(!m_addsDS) {
        createAdditionsDataset();
    }

    if(!m_addsDS) {
        return nullptr;
    }

    std::string attLayerName(attachmentsTableName(name));
    OGRLayer *attLayer = m_addsDS->CreateLayer(attLayerName.c_str(), nullptr,
                                               wkbNone, nullptr);
    if (nullptr == attLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create folder for files
    if(!m_path.empty()) {
        std::string attachmentsPath = File::resetExtension(m_path.c_str(),
                                                    attachmentsFolderExtension());
        if(!Folder::isExists(attachmentsPath)) {
            Folder::mkDir(attachmentsPath);
        }
    }

    // Create table  fields
    OGRFieldDefn fidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn nameField(ATTACH_FILE_NAME_FIELD, OFTString);
    OGRFieldDefn descField(ATTACH_DESCRIPTION_FIELD, OFTString);
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));

    if(attLayer->CreateField(&fidField) != OGRERR_NONE ||
       attLayer->CreateField(&nameField) != OGRERR_NONE ||
       attLayer->CreateField(&descField) != OGRERR_NONE ||
       attLayer->CreateField(&ridField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return attLayer;
}

OGRLayer *DataStore::createEditHistoryTable(const std::string &name)
{
    if(!m_addsDS) {
        createAdditionsDataset();
    }

    if(!m_addsDS) {
        return nullptr;
    }

    std::string logLayerName(historyTableName(name));
    OGRLayer *logLayer = m_addsDS->CreateLayer(logLayerName.c_str(), nullptr,
                                               wkbNone, nullptr);
    if (nullptr == logLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create table  fields
    OGRFieldDefn fidField(FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn afidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn opField(OPERATION_FIELD, OFTInteger64);
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));
    OGRFieldDefn aridField(ATTACHMENT_REMOTE_ID_KEY, OFTInteger64);
    aridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, INIT_RID_COUNTER));

    if(logLayer->CreateField(&fidField) != OGRERR_NONE ||
       logLayer->CreateField(&afidField) != OGRERR_NONE ||
       logLayer->CreateField(&opField) != OGRERR_NONE ||
       logLayer->CreateField(&ridField) != OGRERR_NONE ||
       logLayer->CreateField(&aridField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return logLayer;
}

bool DataStore::isBatchOperation() const
{
    return m_disableJournalCounter > 0;
}

} // namespace ngs

