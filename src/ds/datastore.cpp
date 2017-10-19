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

namespace ngs {

constexpr const char* STORE_EXT = "ngst"; // NextGIS Store
constexpr int STORE_EXT_LEN = length(STORE_EXT);

//------------------------------------------------------------------------------
// DataStore
//------------------------------------------------------------------------------

DataStore::DataStore(ObjectContainer * const parent,
                     const CPLString &name,
                     const CPLString &path) :
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

bool DataStore::isNameValid(const char* name) const
{
    if(nullptr == name || EQUAL(name, ""))
        return false;
    if(EQUALN(name, STORE_EXT, STORE_EXT_LEN))
        return false;

    return Dataset::isNameValid(name);
}

CPLString DataStore::normalizeFieldName(const CPLString& name) const
{
    if(EQUAL(REMOTE_ID_KEY, name)) {
        CPLString out = name + "_";
        return out;
    }
    if(EQUAL("fid", name) || EQUAL("geom", name)) {
        CPLString out = name + "_";
        return out;
    }
    return Dataset::normalizeFieldName(name);
}

void DataStore::fillFeatureClasses()
{
    for(int i = 0; i < m_DS->GetLayerCount(); ++i){
        OGRLayer* layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            const char* layerName = layer->GetName();
            if(EQUAL(layerName, METHADATA_TABLE_NAME)) {
                m_metadata = layer;
                continue;
            }
            if(EQUALN(layerName, OVR_PREFIX, OVR_PREFIX_LEN)) {
                continue;
            }

            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new StoreTable(layer, this,
                                                         layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new StoreFeatureClass(layer, this,
                                                                     layerName)));
            }
        }
    }
}

bool DataStore::create(const char* path)
{
    CPLErrorReset();
    if(nullptr == path || EQUAL(path, "")) {
        return errorMessage(_("The path is empty"));
    }

    GDALDriver* poDriver = Filter::getGDALDriver(CAT_CONTAINER_NGS);
    if(poDriver == nullptr) {
        return errorMessage(_("GeoPackage driver is not present"));
    }

    GDALDataset* DS = poDriver->Create(path, 0, 0, 0, GDT_Unknown, nullptr);
    if(DS == nullptr) {
        return errorMessage(CPLGetLastErrorMsg());
    }

    createMetadataTable(DS);

    GDALClose(DS);
    return true;
}

const char* DataStore::extension()
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

    int version = atoi(property(NGS_VERSION_KEY, "0"));

    if(version < NGS_VERSION_NUM && !upgrade(version)) {
        return errorMessage(_("Upgrade storage failed"));
    }

    return true;
}

FeatureClass* DataStore::createFeatureClass(const CPLString& name,
                                            enum ngsCatalogObjectType /*objectType*/,
                                           OGRFeatureDefn* const definition,
                                           OGRSpatialReference* spatialRef,
                                           OGRwkbGeometryType type,
                                           const Options& options,
                                           const Progress& progress)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened"));
        return nullptr;
    }

    OGRLayer* layer = m_DS->CreateLayer(name, spatialRef, type,
                                        options.getOptions().get());

    if(layer == nullptr) {
        errorMessage(CPLGetLastErrorMsg());
        return nullptr;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn* srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        CPLString newFieldName;
        if(definition->GetFieldCount() - 1 == i) {
            newFieldName = srcField->GetNameRef();
        }
        else {
            newFieldName = normalizeFieldName(srcField->GetNameRef());
            if(!EQUAL(newFieldName, srcField->GetNameRef())) {
                progress.onProgress(COD_WARNING, 0.0,
                                    _("Field %s of source table was renamed to %s in destination tables"),
                                    srcField->GetNameRef(), newFieldName.c_str());
            }
        }

        dstField.SetName(newFieldName);
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(CPLGetLastErrorMsg());
            return nullptr;
        }
    }

    FeatureClass* out = new StoreFeatureClass(layer, this, name);

    if(options.boolOption("CREATE_OVERVIEWS", false) &&
            !options.stringOption("ZOOM_LEVELS", "").empty()) {
        out->createOverviews(progress, options);
    }

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->fullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

Table* DataStore::createTable(const CPLString& name,
                              enum ngsCatalogObjectType /*objectType*/,
                              OGRFeatureDefn* const definition,
                              const Options& options, const Progress& progress)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened"));
        return nullptr;
    }

    OGRLayer* layer = m_DS->CreateLayer(name, nullptr, wkbNone,
                                        options.getOptions().get());

    if(layer == nullptr) {
        errorMessage(CPLGetLastErrorMsg());
        return nullptr;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn* srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        CPLString newFieldName;
        if(definition->GetFieldCount() - 1 == i) {
            newFieldName = srcField->GetNameRef();
        }
        else {
            newFieldName = normalizeFieldName(srcField->GetNameRef());
            if(!EQUAL(newFieldName, srcField->GetNameRef())) {
                progress.onProgress(COD_WARNING, 0.0,
                                    _("Field %s of source table was renamed to %s in destination tables"),
                                    srcField->GetNameRef(), newFieldName.c_str());
            }
        }

        dstField.SetName(newFieldName);
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(CPLGetLastErrorMsg());
            return nullptr;
        }
    }

    Table* out = new StoreTable(layer, this, name);

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->fullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

bool DataStore::setProperty(const char* key, const char* value)
{
    return m_DS->SetMetadataItem(key, value, NG_ADDITIONS_KEY) == OGRERR_NONE;
}

CPLString DataStore::property(const char* key, const char* defaultValue)
{
    const char* out = m_DS->GetMetadataItem(key, NG_ADDITIONS_KEY);
    return nullptr == out ? defaultValue : out;
}

std::map<CPLString, CPLString> DataStore::getProperties(const char* table, const char* domain)
{
    ObjectPtr child = getChild(table);
    Table* tablePtr = ngsDynamicCast(Table, child);
    if(nullptr != tablePtr) // TODO: Fix infinity loop for table
        return tablePtr->properties(domain);
    return std::map<CPLString, CPLString>();
}

bool DataStore::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly())
        return false;
    return type == CAT_FC_GPKG || type == CAT_TABLE_GPKG;

}

bool DataStore::create(const enum ngsCatalogObjectType type,
                       const CPLString& name, const Options& options)
{
    CPLString newName = normalizeDatasetName(name);

    // Get field definitions
    OGRFeatureDefn fieldDefinition(newName);
    int fieldCount = options.intOption("FIELD_COUNT", 0);
    struct fieldData {
        CPLString name, alias;
    };
    std::vector<fieldData> fields;

    for(int i = 0; i < fieldCount; ++i) {
        CPLString fieldName = options.stringOption(CPLSPrintf("FIELD_%d_NAME", i), "");
        if(fieldName.empty()) {
            return errorMessage(_("Name for field %d is not defined"), i);
        }

        CPLString fieldAlias = options.stringOption(CPLSPrintf("FIELD_%d_ALIAS", i), "");
        if(fieldAlias.empty()) {
            fieldAlias = fieldName;
        }
        fieldData data = { fieldName, fieldAlias };
        fields.push_back(data);

        OGRFieldType fieldType = FeatureClass::fieldTypeFromName(
                    options.stringOption(CPLSPrintf("FIELD_%d_TYPE", i), ""));
        OGRFieldDefn field(fieldName, fieldType);
        CPLString defaultValue = options.stringOption(
                    CPLSPrintf("FIELD_%d_DEFAULT_VAL", i), "");
        if(!defaultValue.empty()) {
            field.SetDefault(defaultValue);
        }
        fieldDefinition.AddFieldDefn(&field);
    }

    // Add remote id field
    OGRFieldDefn ridField(REMOTE_ID_KEY, OFTInteger64);
    fieldDefinition.AddFieldDefn(&ridField);

    ObjectPtr object;
    if(type == CAT_FC_GPKG) {
        OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(
                    options.stringOption("GEOMETRY_TYPE", ""));
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

    Table* table = ngsDynamicCast(Table, object);

    // Store aliases and field original names in properties
    for(size_t i = 0; i < fields.size(); ++i) {
        table->setProperty(CPLSPrintf("FIELD_%ld_NAME", i), fields[i].name, NG_ADDITIONS_KEY);
        table->setProperty(CPLSPrintf("FIELD_%ld_ALIAS", i), fields[i].alias, NG_ADDITIONS_KEY);
    }

    bool saveEditHistory = options.boolOption(SAVE_EDIT_HISTORY_KEY, false);
    table->setProperty(SAVE_EDIT_HISTORY_KEY, saveEditHistory ? "ON" : "OFF", NG_ADDITIONS_KEY);

    // Store user defined options in properties
    for(auto it = options.begin(); it != options.end(); ++it) {
        if(EQUALN(it->first, USER_PREFIX_KEY, USER_PREFIX_KEY_LEN)) {
            table->setProperty(CPLSPrintf("%s",
                                       it->first.c_str() + USER_PREFIX_KEY_LEN),
                                       it->second, USER_KEY);
        }
    }

    if(m_childrenLoaded) {
        m_children.push_back(object);
    }

    return true;
}

bool DataStore::upgrade(int /* oldVersion */)
{
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

OGRLayer* DataStore::createAttachmentsTable(const char* name)
{
    if(!m_addsDS) {
        createAdditionsDataset();
    }

    if(!m_addsDS)
        return nullptr;

    CPLString attLayerName(name);
    attLayerName += CPLString("_") + attachmentsFolderExtension();

    OGRLayer* attLayer = m_addsDS->CreateLayer(attLayerName, nullptr, wkbNone, nullptr);
    if (nullptr == attLayer) {
        errorMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create folder for files
    if(nullptr != m_path) {
        CPLString attachmentsPath = CPLResetExtension(m_path,
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

    if(attLayer->CreateField(&fidField) != OGRERR_NONE ||
       attLayer->CreateField(&nameField) != OGRERR_NONE ||
       attLayer->CreateField(&descField) != OGRERR_NONE ||
       attLayer->CreateField(&ridField) != OGRERR_NONE) {
        errorMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return attLayer;
}

} // namespace ngs

