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
#include "geometry.h"
#include "catalog/catalog.h"
#include "catalog/folder.h"
#include "ngstore/version.h"
#include "ngstore/catalog/filter.h"
#include "ngstore/util/constants.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

constexpr const char* STORE_EXT = "ngst"; // NextGIS Store
constexpr int STORE_EXT_LEN = length(STORE_EXT);

constexpr const char* METHADATA_TABLE_NAME = "ngst_meta";
constexpr const char* ATTACHEMENTS_TABLE_NAME = "ngst_attach";
constexpr const char* RASTERS_TABLE_NAME = "ngst_raster";

constexpr const char* NGS_VERSION_KEY = "ngs_version";

// Metadata
constexpr const char* META_KEY = "key";
constexpr unsigned short META_KEY_LIMIT = 128;
constexpr const char* META_VALUE = "value";
constexpr unsigned short META_VALUE_LIMIT = 512;


//------------------------------------------------------------------------------
// Static functions
//------------------------------------------------------------------------------

bool createMetadataTable(GDALDataset* ds)
{
    OGRLayer* pMetadataLayer = ds->CreateLayer(METHADATA_TABLE_NAME, nullptr,
                                                   wkbNone, nullptr);
    if (nullptr == pMetadataLayer) {
        return false;
    }

    OGRFieldDefn oFieldKey(META_KEY, OFTString);
    oFieldKey.SetWidth(META_KEY_LIMIT);
    OGRFieldDefn oFieldValue(META_VALUE, OFTString);
    oFieldValue.SetWidth(META_VALUE_LIMIT);

    if(pMetadataLayer->CreateField(&oFieldKey) != OGRERR_NONE ||
       pMetadataLayer->CreateField(&oFieldValue) != OGRERR_NONE) {
        return false;
    }

    FeaturePtr feature(OGRFeature::CreateFeature(pMetadataLayer->GetLayerDefn()));

    // write version
    feature->SetField(META_KEY, NGS_VERSION_KEY);
    feature->SetField(META_VALUE, NGS_VERSION_NUM);
    return pMetadataLayer->CreateFeature(feature) == OGRERR_NONE ? true : false;
}

/* TODO: add raster support

// Rasters
#define LAYER_URL "url"
#define LAYER_NAME "name"
#define LAYER_ALIAS "alias"
#define LAYER_TYPE "type"
#define LAYER_COPYING "copyright"
#define LAYER_EPSG "epsg"
#define LAYER_MIN_Z "z_min"
#define LAYER_MAX_Z "z_max"
#define LAYER_YORIG_TOP "y_origin_top"
#define LAYER_ACCOUNT "account"

bool createRastersTable(GDALDataset* ds)
{
    OGRLayer* pRasterLayer = ds->CreateLayer(RASTERS_TABLE_NAME, nullptr,
                                                   wkbNone, nullptr);
    if (NULL == pRasterLayer) {
        return true;
    }

    OGRFieldDefn oUrl(LAYER_URL, OFTString);
    OGRFieldDefn oName(LAYER_NAME, OFTString);
    oName.SetWidth(NAME_FIELD_LIMIT);
// FIXME: do we need this field?
    OGRFieldDefn oType(LAYER_TYPE, OFTInteger);
// FIXME: do we need this field?
    OGRFieldDefn oAlias(LAYER_ALIAS, OFTString);
    oName.SetWidth(ALIAS_FIELD_LIMIT);

    OGRFieldDefn oCopyright(LAYER_COPYING, OFTString);
    OGRFieldDefn oEPSG(LAYER_EPSG, OFTInteger);
    OGRFieldDefn oZMin(LAYER_MIN_Z, OFTReal);
    OGRFieldDefn oZMax(LAYER_MAX_Z, OFTReal);
    OGRFieldDefn oYOrigTop(LAYER_YORIG_TOP, OFTInteger);
    oYOrigTop.SetSubType (OFSTBoolean);

    OGRFieldDefn oAccount(LAYER_ACCOUNT, OFTString);
    oAccount.SetWidth(NAME_FIELD_LIMIT);

    OGRFieldDefn oFieldValue(META_VALUE, OFTString);
    oFieldValue.SetWidth(META_VALUE_LIMIT);

    if(pRasterLayer->CreateField(&oUrl) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oName) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oType) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oAlias) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oCopyright) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oEPSG) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oZMin) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oZMax) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oYOrigTop) != OGRERR_NONE ||
       pRasterLayer->CreateField(&oAccount) != OGRERR_NONE) {
        return ngsErrorCodes::EC_CREATE_FAILED;
    }

    return ngsErrorCodes::EC_SUCCESS;
}
*/

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
    Dataset(parent, ngsCatalogObjectType::CAT_CONTAINER_NGS, name, path),
    m_disableJournalCounter(0)
{
    m_storeSpatialRef.importFromEPSG(DEFAULT_EPSG);
}

bool DataStore::isNameValid(const char *name) const
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
                        ngsCatalogObjectType::CAT_TABLE_ANY, layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new FeatureClass(layer, this,
                            ngsCatalogObjectType::CAT_FC_ANY, layerName)));
            }
        }
    }
}

bool DataStore::create(const char *path)
{
    CPLErrorReset();
    if(nullptr == path || EQUAL(path, "")) {
        return errorMessage(ngsErrorCodes::EC_CREATE_FAILED,
                            _("The path is empty"));
    }

    GDALDriver *poDriver = Filter::getGDALDriver(
                ngsCatalogObjectType::CAT_CONTAINER_NGS);
    if(poDriver == nullptr) {
        return errorMessage(ngsErrorCodes::EC_CREATE_FAILED,
                            _("GeoPackage driver is not present"));
    }

    GDALDataset* DS = poDriver->Create(path, 0, 0, 0, GDT_Unknown, nullptr);
    if(DS == nullptr) {
        errorMessage(CPLGetLastErrorMsg());
        return false;
    }

    // Create system tables
    if(!createMetadataTable(DS))
        return errorMessage(ngsErrorCodes::EC_CREATE_FAILED,
                            _("Create metadata table failed"));

    /* TODO: Add raster and attachments support
    if(!createRastersTable(DS))
        return errorMessage(ngsErrorCodes::EC_CREATE_FAILED,
                            _("Create rasters table failed"));

    if(!createAttachmentsTable(DS))
        return errorMessage(ngsErrorCodes::EC_CREATE_FAILED,
                            _("Create attachments table failed"));
    */

    GDALClose(DS);
    return true;
}

const char *DataStore::getExtension()
{
    return STORE_EXT;
}

bool DataStore::open(unsigned int openFlags, const Options &options)
{
    if(!Dataset::open(openFlags, options))
        return false;

    CPLErrorReset();

    // check version and upgrade if needed
    OGRLayer* pMetadataLayer = m_DS->GetLayerByName (METHADATA_TABLE_NAME);
    if(nullptr == pMetadataLayer) {
        return errorMessage(ngsErrorCodes::EC_OPEN_FAILED, _("Invalid structure"));
    }

    pMetadataLayer->ResetReading();
    FeaturePtr feature;
    while( (feature = pMetadataLayer->GetNextFeature()) != nullptr ) {
        if(EQUAL(feature->GetFieldAsString(META_KEY), NGS_VERSION_KEY)) {
            int nVersion = atoi(feature->GetFieldAsString(META_VALUE));
            if(nVersion < NGS_VERSION_NUM) {
                // Upgrade database if needed
                if(!upgrade(nVersion)) {
                    return errorMessage(ngsErrorCodes::EC_OPEN_FAILED,
                                        _("Upgrade storage failed"));
                }
            }
            break;
        }
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
            executeSQL ("PRAGMA synchronous=ON", "SQLITE");
            executeSQL ("PRAGMA journal_mode=ON", "SQLITE");
            executeSQL ("PRAGMA count_changes=ON", "SQLITE");
        }
    }
    else {
        CPLAssert (m_disableJournalCounter < 255); // only 255 layers can simultanious load geodata
        m_disableJournalCounter++;
        if(m_disableJournalCounter == 1) {
            executeSQL ("PRAGMA synchronous=OFF", "SQLITE");
            //executeSQL ("PRAGMA locking_mode=EXCLUSIVE", "SQLITE");
            executeSQL ("PRAGMA journal_mode=OFF", "SQLITE");
            executeSQL ("PRAGMA count_changes=OFF", "SQLITE");
            // executeSQL ("PRAGMA cache_size=15000", "SQLITE");
        }
    }
}

bool DataStore::canDestroy() const
{
    return access(m_path, W_OK) == 0;
}

}

/*
int DataStore::createRemoteTMSRaster(const char *url, const char *name,
                                     const char *alias, const char *copyright,
                                     int epsg, int z_min, int z_max,
                                     bool y_origin_top)
{
    if(!isNameValid(name))
        return ngsErrorCodes::EC_CREATE_FAILED;

    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
    FeaturePtr feature( OGRFeature::CreateFeature(
                pRasterLayer->GetLayerDefn()) );

    feature->SetField (LAYER_URL, url);
    feature->SetField (LAYER_NAME, name);
    // FIXME: do we need this field?
    feature->SetField (LAYER_TYPE, static_cast<int>(ngsDatasetType (Raster)));
    // FIXME: do we need this field?
    feature->SetField (LAYER_ALIAS, alias);
    feature->SetField (LAYER_COPYING, copyright); // show on map copyright string
    feature->SetField (LAYER_EPSG, epsg);
    feature->SetField (LAYER_MIN_Z, z_min);
    feature->SetField (LAYER_MAX_Z, z_max);
    feature->SetField (LAYER_YORIG_TOP, y_origin_top);

    if(pRasterLayer->CreateFeature(feature) != OGRERR_NONE) {
        return ngsErrorCodes::EC_CREATE_FAILED;
    }

    // notify listeners
    if(nullptr != m_notifyFunc)
        m_notifyFunc(ngsSourceCodes::SC_DATA_STORE, RASTERS_TABLE_NAME,
                     NOT_FOUND, ngsChangeCodes::CC_CREATE_RESOURCE);
    return ngsErrorCodes::EC_SUCCESS;
}

DatasetPtr DataStore::getDataset(const CPLString &name)
{
    DatasetPtr outDataset;
    auto it = m_datasets.find (name);
    if( it != m_datasets.end ()){
        if(!it->second->isDeleted ()){
            return it->second;
        }
        else{
            return outDataset;
        }
    }

    OGRLayer* layer = m_DS->GetLayerByName (name);
    if(nullptr != layer){
        Dataset* dataset = nullptr;
        if( layer->GetLayerDefn ()->GetGeomFieldCount () == 0 ){
            dataset = new Table(layer);
        }
        else {
            CPLString historyName = name + HISTORY_APPEND;
            OGRLayer* historyLayer = m_DS->GetLayerByName (historyName);
            StoreFeatureDataset* storeFD = new StoreFeatureDataset(layer);
            // add link to history layer
            storeFD->m_history.reset (new StoreFeatureDataset(historyLayer));
            dataset = storeFD;
        }

        dataset->setNotifyFunc (m_notifyFunc);
        dataset->m_DS = m_DS;
        dataset->m_path = m_path + "/" + name;
        outDataset.reset(dataset);
        if(dataset)
            m_datasets[outDataset->name ()] = outDataset;
    }

    return outDataset;
}

int DataStore::copyDataset(DatasetPtr srcDataset, const CPLString &dstName,
                           LoadData *loadData)
{
    int nRet = ngsErrorCodes::EC_COPY_FAILED;
    if(srcDataset->type () & ngsDatasetType (Table) ||
       srcDataset->type () & ngsDatasetType (Featureset)) {
        // disable journal
        enableJournal(false);
        nRet = DatasetContainer::copyDataset (srcDataset, dstName,
                                              loadData);
        // enable journal
        enableJournal(true);
    }
    else if (srcDataset->type () & ngsDatasetType (Raster)) {
        // TODO: raster. Create raster in DB? Copy/move raster into the folder and store info in DB? Reproject? Band selection?
        // TODO: reproject/or copy and result register in sorage, some checks (SRS, bands, etc.)
        return reportError (ngsErrorCodes::EC_COPY_FAILED, 0,
                            "Unsupported dataset", loadData);
    }
    else {
        return reportError (ngsErrorCodes::EC_COPY_FAILED, 0,
                            "Unsupported dataset", loadData);
    }
    return nRet;
}



DatasetPtr DataStore::createDataset(const CPLString &name,
                                     OGRFeatureDefn * const definition,
                                     const OGRSpatialReference *spatialRef,
                                     OGRwkbGeometryType type,
                                    ProgressInfo *progressInfo)
{
    char** options = nullptr;
    if(progressInfo) {
        progressInfo->onProgress (0, CPLSPrintf ("Create dataset '%s'",
                                                 name.c_str ()));
        options = progressInfo->options ();
    }
    DatasetPtr out;
    OGRLayer *dstLayer = m_DS->CreateLayer(name, &m_storeSpatialRef, type, options);
    if(dstLayer == nullptr) {
        return out;
    }

    // TODO: create ovr layer
    // TODO: create changes layer CPLString changesLayerName = name + "_changes";


    // create history layer
    CPLString historyLayerName = name + HISTORY_APPEND;
    OGRLayer *dstHistoryLayer = m_DS->CreateLayer(historyLayerName,
                                                 &m_storeSpatialRef, type, options);
    if(dstHistoryLayer == nullptr) {
        return out;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) {
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);
        dstField.SetName(normalizeFieldName(srcField->GetNameRef()));
        if (dstLayer->CreateField(&dstField) != OGRERR_NONE) {
            return out;
        }
        if (dstHistoryLayer->CreateField(&dstField) != OGRERR_NONE) {
            return out;
        }
    }

    // TODO: create additional geometry fields for zoom levels: 6 9 12 15 only for dstLayer?
    /*if(type != OGR_GT_Flatten(wkbPoint)) {
        for(auto sampleDist : gSampleDists) {
            OGRFieldDefn dstField(CPLSPrintf ("ngs_geom_%d", sampleDist.second),
                                  OFTBinary);
            if (dstLayer->CreateField(&dstField) != OGRERR_NONE) {
                return out;
            }
        }
    }*//*

    Dataset* dstDataset = nullptr;
    if( type == wkbNone) {
        // TODO: create special class StoreTable instead of table with hostory etc.
        dstDataset = new Table(dstLayer);
    }
    else {
        StoreFeatureDataset* storeFD = new StoreFeatureDataset(dstLayer);
        // add link to history layer
        storeFD->m_history.reset (new StoreFeatureDataset(dstHistoryLayer));
        dstDataset = storeFD;
    }

    dstDataset->setNotifyFunc (m_notifyFunc);
    dstDataset->m_DS = m_DS;
    dstDataset->m_path = m_path + "/" + name;
    out.reset(dstDataset);
    if(dstDataset)
        m_datasets[out->name ()] = out;

    return out;
}
*/
