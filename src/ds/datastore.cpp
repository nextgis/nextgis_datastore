/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
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
#include "catalog/catalog.h"
#include "catalog/folder.h"
#include "geometry.h"
#include "ngstore/catalog/filter.h"
#include "ngstore/codes.h"
#include "ngstore/util/constants.h"
#include "ngstore/version.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

constexpr const char* STORE_EXT = "ngst"; // NextGIS Store
constexpr int STORE_EXT_LEN = length(STORE_EXT);

//------------------------------------------------------------------------------
// Static functions
//------------------------------------------------------------------------------

/* TODO: Add support fo attachments

// Attachments
#define ATTACH_TABLE "table"
#define ATTACH_FEATURE "fid"
#define ATTACH_ID "attid"
#define ATTACH_SIZE "size"
#define ATTACH_FILE_NAME "file_name"
#define ATTACH_FILE_MIME "file_mime"
#define ATTACH_DESCRIPTION "descript"
#define ATTACH_DATA "data"
#define ATTACH_FILE_DATE "date"

bool createAttachmentsTable(GDALDataset* ds)
{
    OGRLayer* pAttachmentsLayer = DS->CreateLayer(ATTACHEMENTS_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pAttachmentsLayer) {
        return ngsErrorCodes::EC_CREATE_FAILED;
    }

    OGRFieldDefn oTable(ATTACH_TABLE, OFTString);
    oTable.SetWidth(NAME_FIELD_LIMIT);
    OGRFieldDefn oFeatureID(ATTACH_FEATURE, OFTInteger64);

    OGRFieldDefn oAttachID(ATTACH_ID, OFTInteger64);
    OGRFieldDefn oAttachSize(ATTACH_SIZE, OFTInteger64);
    OGRFieldDefn oFileName(ATTACH_FILE_NAME, OFTString);
    oFileName.SetWidth(ALIAS_FIELD_LIMIT);
    OGRFieldDefn oMime(ATTACH_FILE_MIME, OFTString);
    oMime.SetWidth(NAME_FIELD_LIMIT);
    OGRFieldDefn oDescription(ATTACH_DESCRIPTION, OFTString);
    oDescription.SetWidth(DESCRIPTION_FIELD_LIMIT);
    OGRFieldDefn oData(ATTACH_DATA, OFTBinary);
    OGRFieldDefn oDate(ATTACH_FILE_DATE, OFTDateTime);

    if(pAttachmentsLayer->CreateField(&oTable) != OGRERR_NONE ||
       pAttachmentsLayer->CreateField(&oFeatureID) != OGRERR_NONE ||
       pAttachmentsLayer->CreateField(&oAttachID) != OGRERR_NONE ||
       pAttachmentsLayer->CreateField(&oAttachSize) != OGRERR_NONE ||
       pAttachmentsLayer->CreateField(&oFileName) != OGRERR_NONE ||
       pAttachmentsLayer->CreateField(&oMime) != OGRERR_NONE ||
       pAttachmentsLayer->CreateField(&oDescription) != OGRERR_NONE ||
       pAttachmentsLayer->CreateField(&oData) != OGRERR_NONE ||
       pAttachmentsLayer->CreateField(&oDate) != OGRERR_NONE) {
        return ngsErrorCodes::EC_CREATE_FAILED;
    }

    return ngsErrorCodes::EC_SUCCESS;
}
*/

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
    int refCount = m_spatialReference->Reference();
    CPLDebug("ngstore", "datastore ref coutnt on init: %d", refCount);
    m_spatialReference->importFromEPSG(DEFAULT_EPSG);
}

DataStore::~DataStore()
{
    if(m_spatialReference->Dereference() == 0)
        delete m_spatialReference;
}

bool DataStore::isNameValid(const char* name) const
{
    if(nullptr == name || EQUAL(name, ""))
        return false;
    if(EQUALN(name, STORE_EXT, STORE_EXT_LEN))
        return false;

    return Dataset::isNameValid(name);
}

void DataStore::fillFeatureClasses()
{
    for(int i = 0; i < m_DS->GetLayerCount(); ++i){
        OGRLayer* layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            const char* layerName = layer->GetName();
            if(EQUALN(layerName, STORE_EXT, STORE_EXT_LEN)) {
                continue;
            }
            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new Table(layer, this,
                        CAT_TABLE_ANY, layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new FeatureClass(layer, this,
                            CAT_FC_ANY, layerName)));
            }
        }
    }
}

bool DataStore::create(const char* path)
{
    CPLErrorReset();
    if(nullptr == path || EQUAL(path, "")) {
        return errorMessage(COD_CREATE_FAILED, _("The path is empty"));
    }

    GDALDriver* poDriver = Filter::getGDALDriver(CAT_CONTAINER_NGS);
    if(poDriver == nullptr) {
        return errorMessage(COD_CREATE_FAILED,
                            _("GeoPackage driver is not present"));
    }

    GDALDataset* DS = poDriver->Create(path, 0, 0, 0, GDT_Unknown, nullptr);
    if(DS == nullptr) {
        errorMessage(CPLGetLastErrorMsg());
        return false;
    }

    // Set user version
    DS->ExecuteSQL(CPLSPrintf("PRAGMA user_version = %d;", NGS_VERSION_NUM), nullptr, nullptr);

    /* TODO: Add attachments support
    // Create system tables
    if(!createMetadataTable(DS))
        return errorMessage(COD_CREATE_FAILED,
                            _("Create metadata table failed"));

    if(!createAttachmentsTable(DS))
        return errorMessage(ngsErrorCodes::EC_CREATE_FAILED,
                            _("Create attachments table failed"));

       // 4.4. create mapping of fields and original spatial reference metadata
    */

    GDALClose(DS);
    return true;
}

const char* DataStore::extension()
{
    return STORE_EXT;
}

bool DataStore::open(unsigned int openFlags, const Options &options)
{
    if(!Dataset::open(openFlags, options))
        return false;

    CPLErrorReset();

    TablePtr userVersion = executeSQL("PRAGMA user_version", "SQLITE");
    userVersion->reset();
    FeaturePtr feature = userVersion->nextFeature();
    int version = 0;
    if(feature) {
        version = feature->GetFieldAsInteger(0);
    }

    if(version < NGS_VERSION_NUM && !upgrade(version)) {
        return errorMessage(COD_OPEN_FAILED, _("Upgrade storage failed"));
    }

    return true;
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
            return errorMessage(COD_CREATE_FAILED,
                                _("Name for field %d is not defined"), i);
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
        fieldDefinition.AddFieldDefn(&field);
    }

    if(type == CAT_FC_GPKG) {
        OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(
                    options.stringOption("GEOMETRY_TYPE", ""));
        if(wkbUnknown == geomType) {
            return errorMessage(COD_CREATE_FAILED, _("Unsupported geometry type"));
        }

        ObjectPtr fc(createFeatureClass(newName, &fieldDefinition,
                                              m_spatialReference, geomType,
                                              options));
        if(!fc) {
            return false;
        }

        if(m_childrenLoaded) {
            m_children.push_back(fc);
        }
    }
    else if(type == CAT_TABLE_GPKG) {
        ObjectPtr t(createTable(newName, &fieldDefinition, options));
        if(nullptr == t) {
            return false;
        }

        if(m_childrenLoaded) {
            m_children.push_back(t);
        }
    }

    // Store aliases and field original names in properties
    for(size_t i = 0; i < fields.size(); ++i) {
        setProperty(CPLSPrintf("%s.FIELD_%ld_NAME", newName.c_str(), i),
                    fields[i].name);
        setProperty(CPLSPrintf("%s.FIELD_%ld_ALIAS", newName.c_str(), i),
                    fields[i].alias);
    }
    // Store other options in properties
    CPLString remoteURL = options.stringOption("SOURCE_URL", "");
    if(!remoteURL.empty()) {
        setProperty(CPLString(newName + ".source_url"), remoteURL);
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
            executeSQL("PRAGMA synchronous=ON", "SQLITE");
            executeSQL("PRAGMA journal_mode=ON", "SQLITE");
            executeSQL("PRAGMA count_changes=ON", "SQLITE");
        }
    }
    else {
        CPLAssert(m_disableJournalCounter < 255); // only 255 layers can simultanious load geodata
        m_disableJournalCounter++;
        if(m_disableJournalCounter == 1) {
            executeSQL("PRAGMA synchronous=OFF", "SQLITE");
            executeSQL("PRAGMA journal_mode=OFF", "SQLITE");
            executeSQL("PRAGMA count_changes=OFF", "SQLITE");
            // executeSQL ("PRAGMA locking_mode=EXCLUSIVE", "SQLITE");
            // executeSQL ("PRAGMA cache_size=15000", "SQLITE");
        }
    }
}

} // namespace ngs
