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
#include "datastore.h"
#include "table.h"
#include "api.h"
#include "rasterdataset.h"
#include "constants.h"

#include "cpl_vsi.h"
#include "cpl_conv.h"
#include "version.h"

#include <iostream>

using namespace ngs;


//------------------------------------------------------------------------------
// DataStore
//------------------------------------------------------------------------------

DataStore::DataStore(const char* path) : m_DS(nullptr), m_notifyFunc(nullptr)
{
    if(nullptr != path)
        m_path = path;
}

DataStore::~DataStore()
{
    GDALClose( m_DS );
}

int DataStore::create(enum StoreType type)
{
    if(nullptr != m_DS)
        return ngsErrorCodes::UNEXPECTED_ERROR;

    if(m_path.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;

    // get GeoPackage driver
    GDALDriver *poDriver = getDriverForType (type);
    if( poDriver == nullptr )
        return ngsErrorCodes::UNSUPPORTED;

    // create ngm.gpkg
    m_DS = poDriver->Create( m_path, 0, 0, 0, GDT_Unknown, nullptr );
    if( m_DS == nullptr ) {
        return ngsErrorCodes::CREATE_FAILED;
    }

    return ngsErrorCodes::SUCCESS;
}

int DataStore::open()
{
    if(m_path.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;

    m_DS = static_cast<GDALDataset*>( GDALOpenEx(m_path,
                    GDAL_OF_SHARED|GDAL_OF_UPDATE, nullptr, nullptr, nullptr) );

    if(nullptr == m_DS)
        return ngsErrorCodes::OPEN_FAILED;

    return ngsErrorCodes::SUCCESS;
}

int DataStore::openOrCreate(enum StoreType type)
{
    if(open() != ngsErrorCodes::SUCCESS)
        return create(type);
    return open();
}

int DataStore::datasetCount() const
{
    return m_DS->GetLayerCount ();
}

DatasetWPtr DataStore::getDataset(const char *name)
{
    DatasetPtr dataset;
    auto it = m_datasources.find (name);
    if( it != m_datasources.end ()){
        if(!it->second->isDeleted ()){
            return it->second;
        }
        else{
            return dataset;
        }
    }

    OGRLayer* pLayer = m_DS->GetLayerByName (name);
    if(nullptr != pLayer){ // TODO: get GDALRaster
        if( EQUAL(pLayer->GetGeometryColumn (), "")){
            dataset.reset (static_cast<Dataset*>(
                               new Table(pLayer, this,
                                       pLayer->GetName (),
                                       pLayer->GetDescription ())));
            m_datasources[dataset->name ()] = dataset;
        }
        else {
            // TODO: Add feature dataset
        }

    }

    return dataset;
}

DatasetWPtr DataStore::getDataset(int index)
{
    for(int i = 0; i < m_DS->GetLayerCount (); ++i){
        OGRLayer* pLayer = m_DS->GetLayer (i);
        if( i == index)
            return getDataset (pLayer->GetName());
    }
    return DatasetWPtr();
}

int DataStore::destroy()
{
    return CPLUnlinkTree (m_path) == 0 ? ngsErrorCodes::SUCCESS :
                                                  ngsErrorCodes::DELETE_FAILED;
}

void DataStore::notifyDatasetChanged(DataStore::ChangeType changeType,
                                    const CPLString &name, long id)
{
    // notify listeners
    if(nullptr != m_notifyFunc) {
        m_notifyFunc(ngsSourceCodes::DATA_STORE, name,
                     id, static_cast<ngsChangeCodes>(changeType));
    }
}

bool DataStore::isNameValid(const CPLString &name) const
{
    if(m_datasources.find (name) != m_datasources.end ())
        return false;
    if(m_DS->GetLayerByName (name) != nullptr)
        return false;

    return true;
}

ResultSetPtr DataStore::executeSQL(const char *statement) const
{
    return ResultSetPtr(m_DS->ExecuteSQL ( statement, nullptr, "" ));
}

GDALDriver *DataStore::getDriverForType(DataStore::StoreType type)
{
    switch (type) {
    case GPKG:
        return GetGDALDriverManager()->GetDriverByName("GPKG");
    }
    return nullptr;
}

void DataStore::setNotifyFunc(ngsNotifyFunc notifyFunc)
{
    m_notifyFunc = notifyFunc;
}

void DataStore::unsetNotifyFunc()
{
    m_notifyFunc = nullptr;
}

int DataStore::destroyDataset(Dataset::Type /*type*/, const CPLString &name)
{
    for (int i = 0; i < m_DS->GetLayerCount(); ++i)
    {
        OGRLayer* poLayer = m_DS->GetLayer(i);
        if(EQUAL(poLayer->GetName(), name.c_str ())) {
            int nRetCode = m_DS->DeleteLayer (i) == OGRERR_NONE ?
                        ngsErrorCodes::SUCCESS :
                        ngsErrorCodes::DELETE_FAILED;
            if(nRetCode == ngsErrorCodes::SUCCESS && nullptr != m_notifyFunc) {
                m_notifyFunc(ngsSourceCodes::DATA_STORE, name.c_str (),
                             NOT_FOUND, ngsChangeCodes::DELETE_RESOURCE);
            }
            return nRetCode;
        }
    }

    return ngsErrorCodes::DELETE_FAILED;
}
//------------------------------------------------------------------------------
// MobileDataStore
//------------------------------------------------------------------------------

MobileDataStore::MobileDataStore(const char* path) : DataStore(path)
{
}

MobileDataStore::~MobileDataStore()
{
}

int MobileDataStore::create(enum StoreType type)
{
    int result = DataStore::create(type);
    if(result != ngsErrorCodes::SUCCESS)
        return result;

    CPLString dir = CPLGetPath (m_path);
    if(dir.empty ()) {
        dir = CPLGetCurrentDir ();
        m_path = CPLFormFilename (dir, m_path, nullptr);
    }


    // create folder if not exist
    if( VSIMkdir( dir, 0755 ) != 0 ) {
        return ngsErrorCodes::CREATE_FAILED;
    }

    // create folder for external images and other stuff if not exist
    // if db name is ngs.gpkg folder shoudl be names ngs.data
    CPLString baseName = CPLGetBasename (m_path);
    m_dataPath = CPLFormFilename(dir, baseName, "data");
    if( VSIMkdir( m_dataPath.c_str (), 0755 ) != 0 ) {
        return ngsErrorCodes::CREATE_FAILED;
    }

    // create system tables
    int nResult;
    nResult = createMetadataTable ();
    if(nResult != ngsErrorCodes::SUCCESS)
        return nResult;
    nResult = createRastersTable ();
    if(nResult != ngsErrorCodes::SUCCESS)
        return nResult;
    nResult = createAttachmentsTable ();
    if(nResult != ngsErrorCodes::SUCCESS)
        return nResult;
    return ngsErrorCodes::SUCCESS;
}

int MobileDataStore::open()
{
    if(m_path.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;
    if (CPLCheckForFile(const_cast<char*>(m_path.c_str ()), nullptr) != TRUE) {
        return ngsErrorCodes::INVALID;
    }

    m_DS = static_cast<GDALDataset*>( GDALOpenEx(m_path,
                      GDAL_OF_SHARED|GDAL_OF_UPDATE, nullptr, nullptr, nullptr) );

    if(nullptr == m_DS)
        return ngsErrorCodes::OPEN_FAILED;

    // check version and upgrade if needed
    OGRLayer* pMetadataLayer = m_DS->GetLayerByName (METHADATA_TABLE_NAME);
    if(nullptr == pMetadataLayer)
        return ngsErrorCodes::INVALID;

    pMetadataLayer->ResetReading();
    FeaturePtr feature;
    while( (feature = pMetadataLayer->GetNextFeature()) != nullptr ) {
        if(EQUAL(feature->GetFieldAsString(META_KEY), NGS_VERSION_KEY)) {
            int nVersion = atoi(feature->GetFieldAsString(META_VALUE));
            if(nVersion < NGS_VERSION_NUM) {
                int nResult = upgrade (nVersion);
                if(nResult != ngsErrorCodes::SUCCESS)
                    return nResult;
            }
            break;
        }
    }

    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
    if(nullptr == pRasterLayer)
        return ngsErrorCodes::INVALID;

    return ngsErrorCodes::SUCCESS;
}

int MobileDataStore::createRemoteTMSRaster(const char *url, const char *name,
                                     const char *alias, const char *copyright,
                                     int epsg, int z_min, int z_max,
                                     bool y_origin_top)
{
    if(!isNameValid(name))
        return ngsErrorCodes::CREATE_FAILED;

    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
    FeaturePtr feature( OGRFeature::CreateFeature(
                pRasterLayer->GetLayerDefn()) );

    feature->SetField (LAYER_URL, url);
    feature->SetField (LAYER_NAME, name);
    feature->SetField (LAYER_TYPE, ngs::Dataset::REMOTE_TMS);
    feature->SetField (LAYER_ALIAS, alias);
    feature->SetField (LAYER_COPYING, copyright);
    feature->SetField (LAYER_EPSG, epsg);
    feature->SetField (LAYER_MIN_Z, z_min);
    feature->SetField (LAYER_MAX_Z, z_max);
    feature->SetField (LAYER_YORIG_TOP, y_origin_top);

    if(pRasterLayer->CreateFeature(feature) != OGRERR_NONE) {
        return ngsErrorCodes::CREATE_FAILED;
    }

    // notify listeners
    if(nullptr != m_notifyFunc)
        m_notifyFunc(ngsSourceCodes::DATA_STORE, RASTERS_TABLE_NAME,
                     NOT_FOUND, ngsChangeCodes::CREATE_RESOURCE);
    return ngsErrorCodes::SUCCESS;
}

int MobileDataStore::datasetCount() const
{
    int dsLayerCount = m_DS->GetLayerCount ();
    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
    dsLayerCount += pRasterLayer->GetFeatureCount ();
    return dsLayerCount - SYS_TABLE_COUNT;
}

DatasetWPtr MobileDataStore::getDataset(const char *name)
{
    DatasetPtr dataset;
    auto it = m_datasources.find (name);
    if( it != m_datasources.end ()){
        if(!it->second->isDeleted ()){
            return it->second;
        }
        else{
            return dataset;
        }
    }

    OGRLayer* pLayer = m_DS->GetLayerByName (name);
    if(nullptr != pLayer){ // TODO: get GDALRaster
        if( EQUAL(pLayer->GetGeometryColumn (), "")){
            dataset.reset (static_cast<Dataset*>(
                               new Table(pLayer, this,
                                       pLayer->GetName (),
                                       pLayer->GetDescription ())));
            m_datasources[dataset->name ()] = dataset;
        }
        else {
            // TODO: Add feature dataset
        }

        return dataset;
    }

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
    }
    return dataset;
}

DatasetWPtr MobileDataStore::getDataset(int index)
{
    int dsLayers = m_DS->GetLayerCount () - SYS_TABLE_COUNT;
    if(index < dsLayers){
        int counter = 0;
        for(int i = 0; i < m_DS->GetLayerCount (); ++i){
            OGRLayer* pLayer = m_DS->GetLayer (i);
            // skip system tables
            if(EQUAL (pLayer->GetName (), METHADATA_TABLE_NAME) ||
               EQUAL (pLayer->GetName (), ATTACHEMENTS_TABLE_NAME) ||
               EQUAL (pLayer->GetName (), RASTERS_TABLE_NAME) ||
               EQUAL (pLayer->GetName (), MAPS_TABLE_NAME) )
                continue;
            if(counter == index)
                return getDataset (pLayer->GetName());
            counter++;
        }
    }
    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
    FeaturePtr feature( pRasterLayer->GetFeature (index - dsLayers) );
    if(nullptr == feature)
        return DatasetWPtr();
    return getDataset (feature->GetFieldAsString (LAYER_NAME));
}

int MobileDataStore::destroy()
{
    // Unlink some folders with external rasters adn etc.
    if(!m_dataPath.empty()) {
        if(CPLUnlinkTree(m_dataPath) != 0){
            return ngsErrorCodes::DELETE_FAILED;
        }
    }
    return CPLUnlinkTree (m_path) == 0 ? ngsErrorCodes::SUCCESS :
                                                  ngsErrorCodes::DELETE_FAILED;
}

int MobileDataStore::createMetadataTable()
{
    OGRLayer* pMetadataLayer = m_DS->CreateLayer(METHADATA_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pMetadataLayer) {
        return ngsErrorCodes::CREATE_FAILED;
    }

    OGRFieldDefn oFieldKey(META_KEY, OFTString);
    oFieldKey.SetWidth(META_KEY_LIMIT);
    OGRFieldDefn oFieldValue(META_VALUE, OFTString);
    oFieldValue.SetWidth(META_VALUE_LIMIT);

    if(pMetadataLayer->CreateField(&oFieldKey) != OGRERR_NONE ||
       pMetadataLayer->CreateField(&oFieldValue) != OGRERR_NONE) {
        return ngsErrorCodes::CREATE_FAILED;
    }

    FeaturePtr feature( OGRFeature::CreateFeature(
                pMetadataLayer->GetLayerDefn()) );

    // write version
    feature->SetField(META_KEY, NGS_VERSION_KEY);
    feature->SetField(META_VALUE, NGS_VERSION_NUM);
    if(pMetadataLayer->CreateFeature(feature) != OGRERR_NONE) {
        return ngsErrorCodes::CREATE_FAILED;
    }

    return ngsErrorCodes::SUCCESS;
}

int MobileDataStore::createRastersTable()
{
    OGRLayer* pRasterLayer = m_DS->CreateLayer(RASTERS_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pRasterLayer) {
        return ngsErrorCodes::CREATE_FAILED;
    }

    OGRFieldDefn oUrl(LAYER_URL, OFTString);
    OGRFieldDefn oName(LAYER_NAME, OFTString);
    oName.SetWidth(NAME_FIELD_LIMIT);

    OGRFieldDefn oType(LAYER_TYPE, OFTInteger);

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
        return ngsErrorCodes::CREATE_FAILED;
    }

    return ngsErrorCodes::SUCCESS;
}

int MobileDataStore::createAttachmentsTable()
{
    OGRLayer* pAttachmentsLayer = m_DS->CreateLayer(ATTACHEMENTS_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pAttachmentsLayer) {
        return ngsErrorCodes::CREATE_FAILED;
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
        return ngsErrorCodes::CREATE_FAILED;
    }

    return ngsErrorCodes::SUCCESS;
}

int MobileDataStore::upgrade(int /* oldVersion */)
{
    // no structure changes for version 1
    return ngsErrorCodes::SUCCESS;
}

int MobileDataStore::destroyDataset(Dataset::Type type, const CPLString &name)
{
    switch (type) {
    case Dataset::REMOTE_TMS:
    case Dataset::NGW_IMAGE:
        CPLErrorReset();
        {
        CPLString statement("DELETE FROM " RASTERS_TABLE_NAME " WHERE "
                         LAYER_NAME " = '");
        statement += name + "'";
        executeSQL ( statement );
        }
        if (CPLGetLastErrorNo() == CE_None) {
            if(nullptr != m_notifyFunc) {
                m_notifyFunc(ngsSourceCodes::DATA_STORE, name,
                             NOT_FOUND, ngsChangeCodes::DELETE_RESOURCE);
            }
            return ngsErrorCodes::SUCCESS;
        }
        return ngsErrorCodes::DELETE_FAILED;

    case Dataset::LOCAL_RASTER:
    case Dataset::LOCAL_TMS:
        for (int i = 0; i < m_DS->GetLayerCount(); ++i)
        {
            OGRLayer* poLayer = m_DS->GetLayer(i);
            if(EQUAL(poLayer->GetName(), name.c_str ())) {
                int nRetCode = m_DS->DeleteLayer (i) == OGRERR_NONE ?
                            ngsErrorCodes::SUCCESS :
                            ngsErrorCodes::DELETE_FAILED;
                if(nRetCode == ngsErrorCodes::SUCCESS && nullptr != m_notifyFunc) {
                    m_notifyFunc(ngsSourceCodes::DATA_STORE, name.c_str (),
                                 NOT_FOUND, ngsChangeCodes::DELETE_RESOURCE);
                }
                return nRetCode;
            }
        }
    }

    return ngsErrorCodes::DELETE_FAILED;
}

bool MobileDataStore::isNameValid(const CPLString &name) const
{
    if(name.size () < 4 || name.size () >= NAME_FIELD_LIMIT)
        return false;
    if((name[0] == 'n' || name[0] == 'N') &&
       (name[1] == 'g' || name[1] == 'G') &&
       (name[2] == 's' || name[2] == 'S') &&
       (name[3] == '_'))
        return false;
    if(m_datasources.find (name) != m_datasources.end ())
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

ResultSetPtr MobileDataStore::executeSQL(const char *statement) const
{
    return ResultSetPtr(m_DS->ExecuteSQL ( statement, nullptr, "SQLITE" ));
}

void MobileDataStore::onLowMemory()
{
    // free all cached datasources
    m_datasources.clear ();
}
