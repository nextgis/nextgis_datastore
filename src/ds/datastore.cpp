/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
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
#include "api_priv.h"
#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "datastore.h"
#include <iostream>
#include "rasterdataset.h"
#include "storefeaturedataset.h"
#include "util/constants.h"
#include "util/geometryutil.h"
#include "version.h"

#define NGS_DATA_FOLDER "data"

using namespace ngs;

//------------------------------------------------------------------------------
// DataStore
//------------------------------------------------------------------------------

DataStore::DataStore() : DatasetContainer(), m_disableJournalCounter(0)
{
    m_type = ngsDatasetType(Store) | ngsDatasetType(Container);
    m_storeSpatialRef.importFromEPSG(DEFAULT_EPSG);
}

DataStore::~DataStore()
{
}

DataStorePtr DataStore::create(const CPLString &path)
{
    CPLErrorReset();
    DataStorePtr out;
    if(path.empty()) {
        CPLError(CE_Failure, CPLE_AppDefined, "Path not specified.");
        return out;
    }

    CPLString absPath = path;
    CPLString dir = CPLGetPath (absPath);
    if(dir.empty ()) {
        dir = CPLGetCurrentDir ();
        absPath = CPLFormFilename (dir, absPath, nullptr);
    }

    // create folder if not exist
    if( CPLCheckForFile (const_cast<char*>(dir.c_str ()), nullptr) == FALSE &&
            VSIMkdir( dir, 0755 ) != 0 ) {
        CPLError(CE_Failure, CPLE_AppDefined, "Create datastore failed.");
        return out;
    }

    // get GeoPackage driver
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
    if( poDriver == nullptr ) {
        CPLError(CE_Failure, CPLE_AppDefined, "GeoPackage driver is not present.");
        return out;
    }

    // create
    GDALDatasetPtr DS (poDriver->Create( path, 0, 0, 0, GDT_Unknown, nullptr ));
    if( DS == nullptr ) {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset create failed.");
        return out;
    }

    // create folder for external images and other stuff if not exist
    // if db name is ngs.gpkg folder shoudl be names ngs.data
    CPLString baseName = CPLGetBasename (absPath);
    CPLString dataPath = CPLFormFilename(dir, baseName, NGS_DATA_FOLDER);
    if( CPLCheckForFile (const_cast<char*>(dataPath.c_str ()), nullptr) == FALSE) {
        if( VSIMkdir( dataPath.c_str (), 0755 ) != 0 ) {
            CPLError(CE_Failure, CPLE_AppDefined, "Create datafolder failed.");
            return out;
        }
    }

    // create system tables
    int nResult;
    nResult = createMetadataTable (DS);
    if(nResult != ngsErrorCodes::EC_SUCCESS)
        return out;
    nResult = createRastersTable (DS);
    if(nResult != ngsErrorCodes::EC_SUCCESS)
        return out;
    nResult = createAttachmentsTable (DS);
    if(nResult != ngsErrorCodes::EC_SUCCESS)
        return out;

    DataStore *outDS = new DataStore();

    outDS->m_DS = DS;
    outDS->m_path = path;
    outDS->m_opened = true;
    outDS->m_dataPath = dataPath;

    out.reset (outDS);

    return out;
}

DataStorePtr DataStore::open(const CPLString &path)
{
    CPLErrorReset();
    DataStorePtr out;
    if(path.empty()) {
        CPLError(CE_Failure, CPLE_AppDefined, "Path not specified.");
        return out;
    }

    if (CPLCheckForFile(const_cast<char*>(path.c_str ()), nullptr) != TRUE) {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid path.");
        return out;

    }

    GDALDatasetPtr DS (static_cast<GDALDataset*>( GDALOpenEx(path,
                       GDAL_OF_SHARED|GDAL_OF_UPDATE, nullptr, nullptr, nullptr)));

    if( DS == nullptr ) {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset create failed.");
        return out;
    }

    // check version and upgrade if needed
    OGRLayer* pMetadataLayer = DS->GetLayerByName (METHADATA_TABLE_NAME);
    if(nullptr == pMetadataLayer) {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid structure.");
        return out;
    }

    pMetadataLayer->ResetReading();
    FeaturePtr feature;
    while( (feature = pMetadataLayer->GetNextFeature()) != nullptr ) {
        if(EQUAL(feature->GetFieldAsString(META_KEY), NGS_VERSION_KEY)) {
            int nVersion = atoi(feature->GetFieldAsString(META_VALUE));
            if(nVersion < NGS_VERSION_NUM) {
                int nResult = upgrade (nVersion, DS);
                if(nResult != ngsErrorCodes::EC_SUCCESS) {
                    CPLError(CE_Failure, CPLE_AppDefined, "Upgrade DB failed.");
                    return out;
                }
            }
            break;
        }
    }

    OGRLayer* pRasterLayer = DS->GetLayerByName (RASTERS_TABLE_NAME);
    if(nullptr == pRasterLayer) {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid structure.");
        return out;
    }

    DataStore *outDS = new DataStore();

    outDS->m_DS = DS;
    outDS->m_path = path;
    outDS->m_opened = true;
    outDS->setDataPath ();
    out.reset (outDS);

    return out;
}

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

int DataStore::datasetCount() const
{
    // each layer have history table. Count is (dsLayerCount - SYS_TABLE_COUNT) / 2
    int dsLayerCount = m_DS->GetLayerCount ();
    return (dsLayerCount - SYS_TABLE_COUNT) / 2;
}

int DataStore::rasterCount() const
{
    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
    if(nullptr == pRasterLayer)
        return 0;
    return DatasetContainer::rasterCount () + static_cast<int>(
                pRasterLayer->GetFeatureCount ());
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

DatasetPtr DataStore::getDataset(int index)
{
    int dsLayers = m_DS->GetLayerCount () - SYS_TABLE_COUNT;
    if(index < dsLayers){
        int counter = 0;
        for(int i = 0; i < m_DS->GetLayerCount (); ++i){
            OGRLayer* pLayer = m_DS->GetLayer (i);
            // skip system tables
            if(EQUAL (pLayer->GetName (), METHADATA_TABLE_NAME) ||
               EQUAL (pLayer->GetName (), ATTACHEMENTS_TABLE_NAME) ||
               EQUAL (pLayer->GetName (), RASTERS_TABLE_NAME) )
                continue;
            if(counter == index)
                return getDataset(pLayer->GetName());
            counter++;
        }
    }
    return DatasetPtr();
}

DatasetPtr DataStore::createDataset(const CPLString &name,
                                     OGRFeatureDefn * const definition,
                                     const OGRSpatialReference */*spatialRef*/,
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
    }*/

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

/* TODO: getRaster(int index)
 OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
 FeaturePtr feature( pRasterLayer->GetFeature (index - dsLayers) );
 if(nullptr == feature)
     return DatasetWPtr();
return getDataset (feature->GetFieldAsString (LAYER_NAME));
OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
pRasterLayer->ResetReading ();
FeaturePtr feature;
while( (feature = pRasterLayer->GetNextFeature()) != nullptr ) {
    if(EQUAL(feature->GetFieldAsString (LAYER_NAME), name)){

        switch(feature->GetFieldAsInteger (LAYER_TYPE)){
        case Dataset::REMOTE_TMS:
            dataset.reset (static_cast<Dataset*>(new RemoteTMSDataset(this,
                                    feature->GetFieldAsString (LAYER_NAME),
                                    feature->GetFieldAsString (LAYER_ALIAS))));
            m_datasources[dataset->name ()] = dataset;
            break;
        }

        return dataset;
    }
}*/

int DataStore::destroy(ProgressInfo *progressInfo)
{
    setDataPath ();
    // Unlink some folders with external rasters adn etc.
    if(!m_dataPath.empty()) {
        if(CPLUnlinkTree(m_dataPath) != 0){
            if(progressInfo)
                progressInfo->setStatus (ngsErrorCodes::EC_DELETE_FAILED);
            return ngsErrorCodes::EC_DELETE_FAILED;
        }
    }
    return DatasetContainer::destroy (progressInfo);
}

int DataStore::createMetadataTable(GDALDatasetPtr DS)
{
    OGRLayer* pMetadataLayer = DS->CreateLayer(METHADATA_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pMetadataLayer) {
        return ngsErrorCodes::EC_CREATE_FAILED;
    }

    OGRFieldDefn oFieldKey(META_KEY, OFTString);
    oFieldKey.SetWidth(META_KEY_LIMIT);
    OGRFieldDefn oFieldValue(META_VALUE, OFTString);
    oFieldValue.SetWidth(META_VALUE_LIMIT);

    if(pMetadataLayer->CreateField(&oFieldKey) != OGRERR_NONE ||
       pMetadataLayer->CreateField(&oFieldValue) != OGRERR_NONE) {
        return ngsErrorCodes::EC_CREATE_FAILED;
    }

    FeaturePtr feature( OGRFeature::CreateFeature(
                pMetadataLayer->GetLayerDefn()) );

    // write version
    feature->SetField(META_KEY, NGS_VERSION_KEY);
    feature->SetField(META_VALUE, NGS_VERSION_NUM);
    if(pMetadataLayer->CreateFeature(feature) != OGRERR_NONE) {
        return ngsErrorCodes::EC_CREATE_FAILED;
    }
    return ngsErrorCodes::EC_SUCCESS;
}

int DataStore::createRastersTable(GDALDatasetPtr DS)
{
    OGRLayer* pRasterLayer = DS->CreateLayer(RASTERS_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pRasterLayer) {
        return ngsErrorCodes::EC_CREATE_FAILED;
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

int DataStore::createAttachmentsTable(GDALDatasetPtr DS)
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

int DataStore::upgrade(int /* oldVersion */, GDALDatasetPtr /*ds*/)
{
    // no structure changes for version 1
    return ngsErrorCodes::EC_SUCCESS;
}

void DataStore::setDataPath()
{
    if(m_dataPath.empty ()) {
        CPLString baseName = CPLGetBasename (m_path);
        CPLString dir = CPLGetPath (m_path);
        m_dataPath = CPLFormFilename(dir, baseName, "data");
        if( CPLCheckForFile (const_cast<char*>(m_dataPath.c_str ()), nullptr)
                == FALSE)
            m_dataPath.clear();
    }
}

bool DataStore::isNameValid(const CPLString &name) const
{
    if(name.size () < 4 || name.size () >= NAME_FIELD_LIMIT)
        return false;
    if((name[0] == 'n' || name[0] == 'N') &&
       (name[1] == 'g' || name[1] == 'G') &&
       (name[2] == 's' || name[2] == 'S') &&
       (name[3] == '_'))
        return false;
    if(m_datasets.find (name) != m_datasets.end ())
        return false;
    if(m_DS->GetLayerByName (name) != nullptr)
        return false;

    CPLString statement("SELECT count(*) FROM " RASTERS_TABLE_NAME " WHERE "
                     LAYER_NAME " = '");
    statement += name + "'";
    ResultSetPtr tmpLayer = executeSQL ( statement );
    FeaturePtr feature = tmpLayer->GetFeature (0);
    if(feature->GetFieldAsInteger (0) > 0)
        return false;
    return true;
}

void DataStore::enableJournal(bool enable)
{
    if(enable) {        
        m_disableJournalCounter--;
        if(m_disableJournalCounter == 0) {
            executeSQL ("PRAGMA synchronous=ON");
            executeSQL ("PRAGMA journal_mode=ON");
            executeSQL ("PRAGMA count_changes=ON");
        }
    }
    else {
        CPLAssert (m_disableJournalCounter < 255); // only 255 layers can simultanious load geodata
        m_disableJournalCounter++;
        if(m_disableJournalCounter == 1) {
            executeSQL ("PRAGMA synchronous=OFF");
            //executeSQL ("PRAGMA locking_mode=EXCLUSIVE");
            executeSQL ("PRAGMA journal_mode=OFF");
            executeSQL ("PRAGMA count_changes=OFF");
            // executeSQL ("PRAGMA cache_size=15000");
        }
    }
}

ResultSetPtr DataStore::executeSQL(const CPLString &statement) const
{
    return ResultSetPtr(m_DS->ExecuteSQL(statement, nullptr, "SQLITE"),
                                     [=](OGRLayer* layer)
    {
        m_DS->ReleaseResultSet(layer);
    });
}

void DataStore::onLowMemory()
{
    // free all cached datasources
    m_datasets.clear ();
}

DataStorePtr DataStore::openOrCreate(const CPLString& path)
{
    DataStorePtr out;
    out = open( path );
    if( nullptr == out)
        return create(path);
    return out;
}


const char *DataStore::getOptions(ngsDataStoreOptionsTypes optionType) const
{
    switch (optionType) {
    case OT_CREATE_DATASOURCE:
    case OT_OPEN:
    case OT_CREATE_DATASET:
        return DatasetContainer::getOptions (optionType);
    case OT_LOAD:
        return "<LoadOptionList>"
               "  <Option name='LOAD_OP' type='string-select' description='select load operation' default='COPY'>"
               "    <Value>COPY</Value>"
               "    <Value>MOVE</Value>"
               "  </Option>"
               "  <Option name='FEATURES_SKIP' type='string-select' description='skip features during loading' default='NO'>"
               "    <Value>NO</Value>"
               "    <Value>EMPTY_GEOMETRY</Value>"
               "    <Value>INVALID_GEOMETRY</Value>"
               "  </Option>"
               "</LoadOptionList>";
    }
}
