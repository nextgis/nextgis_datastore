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


#if defined(WIN32) || defined(_WIN32)
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

#define DEFAULT_CACHE PATH_SEPARATOR ".cache"
#define DEFAULT_DATA PATH_SEPARATOR ".data"

using namespace ngs;


//------------------------------------------------------------------------------
// DataStore
//------------------------------------------------------------------------------

DataStore::DataStore(const char* path, const char* dataPath,
                     const char* cachePath) : m_DS(nullptr)
{
    if(nullptr != path)
        m_path = path;
    if(nullptr == cachePath) {
        if(nullptr != path) {
            m_cachePath = path;
            m_cachePath += DEFAULT_CACHE;
        }
    }
    else {
        m_cachePath = cachePath;
    }
    if(nullptr == dataPath) {
        if(nullptr != path) {
            m_dataPath = path;
            m_dataPath += DEFAULT_DATA;
        }
    }
    else {
        m_dataPath = dataPath;
    }

    m_driversLoaded = false;
}

DataStore::~DataStore()
{
    GDALClose( m_DS );

    GDALDestroyDriverManager();
}

int DataStore::create()
{
    if(nullptr != m_DS)
        return ngsErrorCodes::UNEXPECTED_ERROR;

    if(m_path.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;

    initGDAL();
    // get GeoPackage driver
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
    if( poDriver == nullptr ) {
        return ngsErrorCodes::UNSUPPORTED;
    }

    // create folder if not exist
    if( VSIMkdir( m_path.c_str (), 0755 ) != 0 ) {
        return ngsErrorCodes::CREATE_DIR_FAILED;
    }

    // create folder if not exist
    if( VSIMkdir( m_cachePath.c_str (), 0755 ) != 0 ) {
        return ngsErrorCodes::CREATE_DIR_FAILED;
    }

    string sFullPath = m_path + string(PATH_SEPARATOR MAIN_DATABASE);

    // create ngm.gpkg
    m_DS = poDriver->Create( sFullPath.c_str (), 0, 0, 0,
                                          GDT_Unknown, nullptr );
    if( m_DS == nullptr ) {
        return ngsErrorCodes::CREATE_DB_FAILED;
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
    nResult = createMapsTable ();
    if(nResult != ngsErrorCodes::SUCCESS)
        return nResult;
    nResult = createLayersTable ();
    if(nResult != ngsErrorCodes::SUCCESS)
        return nResult;
    return ngsErrorCodes::SUCCESS;
}

int DataStore::open()
{
    if(m_path.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;
    string sFullPath = m_path + string(PATH_SEPARATOR MAIN_DATABASE);
    if (CPLCheckForFile(const_cast<char*>(sFullPath.c_str()), nullptr) != TRUE) {
        return ngsErrorCodes::INVALID_PATH;
    }

    initGDAL();
    // get GeoPackage driver
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
    if( poDriver == nullptr ) {
        return ngsErrorCodes::UNSUPPORTED;
    }

    m_DS = static_cast<GDALDataset*>( GDALOpenEx(sFullPath.c_str (),
                                                   GDAL_OF_SHARED|GDAL_OF_UPDATE,
                                                   nullptr, nullptr, nullptr) );

    // check version and upgrade if needed
    OGRLayer* pMetadataLayer = m_DS->GetLayerByName (METHADATA_TABLE_NAME);
    if(nullptr == pMetadataLayer)
        return ngsErrorCodes::INVALID_STUCTURE;

    pMetadataLayer->ResetReading();
    FeaturePtr feature;
    while( (feature = pMetadataLayer->GetNextFeature()) != nullptr ) {
        if(EQUAL(feature->GetFieldAsString(META_KEY), NGS_VERSION_KEY)) {
            int nVersion = atoi(feature->GetFieldAsString(META_VALUE));
            if(nVersion < NGS_VERSION) {
                int nResult = upgrade (nVersion);
                if(nResult != ngsErrorCodes::SUCCESS)
                    return nResult;
            }
            break;
        }
    }

    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
    if(nullptr == pRasterLayer)
        return ngsErrorCodes::INVALID_STUCTURE;

    return ngsErrorCodes::SUCCESS;
}

int DataStore::openOrCreate()
{
    if(open() != ngsErrorCodes::SUCCESS)
        return create();
    return open();
}

int DataStore::createRemoteTMSRaster(const char *url, const char *name,
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

    // TODO: notify listeners

    return ngsErrorCodes::SUCCESS;
}

int DataStore::datasetCount() const
{
    int dsLayerCount = m_DS->GetLayerCount ();
    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTERS_TABLE_NAME);
    dsLayerCount += pRasterLayer->GetFeatureCount ();
    return dsLayerCount - SYS_TABLE_COUNT;
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

DatasetWPtr DataStore::getDataset(int index)
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

string DataStore::formats()
{
    string out;
    registerDrivers();
    for( int iDr = 0; iDr < GDALGetDriverCount(); iDr++ ) {
        GDALDriverH hDriver = GDALGetDriver(iDr);

        const char *pszRFlag = "", *pszWFlag, *pszVirtualIO, *pszSubdatasets,
                *pszKind;
        char** papszMD = GDALGetMetadata( hDriver, NULL );

        if( CSLFetchBoolean( papszMD, GDAL_DCAP_OPEN, FALSE ) )
            pszRFlag = "r";

        if( CSLFetchBoolean( papszMD, GDAL_DCAP_CREATE, FALSE ) )
            pszWFlag = "w+";
        else if( CSLFetchBoolean( papszMD, GDAL_DCAP_CREATECOPY, FALSE ) )
            pszWFlag = "w";
        else
            pszWFlag = "o";

        if( CSLFetchBoolean( papszMD, GDAL_DCAP_VIRTUALIO, FALSE ) )
            pszVirtualIO = "v";
        else
            pszVirtualIO = "";

        if( CSLFetchBoolean( papszMD, GDAL_DMD_SUBDATASETS, FALSE ) )
            pszSubdatasets = "s";
        else
            pszSubdatasets = "";

        if( CSLFetchBoolean( papszMD, GDAL_DCAP_RASTER, FALSE ) &&
            CSLFetchBoolean( papszMD, GDAL_DCAP_VECTOR, FALSE ))
            pszKind = "raster,vector";
        else if( CSLFetchBoolean( papszMD, GDAL_DCAP_RASTER, FALSE ) )
            pszKind = "raster";
        else if( CSLFetchBoolean( papszMD, GDAL_DCAP_VECTOR, FALSE ) )
            pszKind = "vector";
        else if( CSLFetchBoolean( papszMD, GDAL_DCAP_GNM, FALSE ) )
            pszKind = "geography network";
        else
            pszKind = "unknown kind";

        out += CPLSPrintf( "  %s -%s- (%s%s%s%s): %s\n",
                GDALGetDriverShortName( hDriver ),
                pszKind,
                pszRFlag, pszWFlag, pszVirtualIO, pszSubdatasets,
                GDALGetDriverLongName( hDriver ) );
    }

    return out;
}

void DataStore::initGDAL()
{
    // set config options
    CPLSetConfigOption("GDAL_DATA", m_dataPath.c_str());
    CPLSetConfigOption("GDAL_CACHEMAX", CACHEMAX);
    CPLSetConfigOption("GDAL_HTTP_USERAGENT", NGM_USERAGENT);
    CPLSetConfigOption("CPL_CURL_GZIP", HTTP_USE_GZIP);
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", HTTP_TIMEOUT);
    CPLSetConfigOption("GDAL_DEFAULT_WMS_CACHE_PATH", m_cachePath.c_str());

#ifdef _DEBUG
    cout << "HTTP user agent set to: " << NGM_USERAGENT << endl;
#endif //_DEBUG

    // register drivers
    registerDrivers ();
}

int DataStore::destroy()
{
    if(m_path.empty())
        return ngsErrorCodes::INVALID_PATH;
    if(!m_cachePath.empty()) {
        if(CPLUnlinkTree(m_cachePath.c_str()) != 0){
            return ngsErrorCodes::DELETE_FAILED;
        }
    }
    return CPLUnlinkTree (m_path.c_str()) == 0 ? ngsErrorCodes::SUCCESS :
                                                  ngsErrorCodes::DELETE_FAILED;
}

void DataStore::registerDrivers()
{
    if(m_driversLoaded)
        return;
    GDALRegister_VRT();
    GDALRegister_GTiff();
    GDALRegister_HFA();
    GDALRegister_PNG();
    GDALRegister_JPEG();
    GDALRegister_MEM();
    RegisterOGRShape();
    RegisterOGRTAB();
    RegisterOGRVRT();
    RegisterOGRMEM();
    RegisterOGRGPX();
    RegisterOGRKML();
    RegisterOGRGeoJSON();
    RegisterOGRGeoPackage();
    RegisterOGRSQLite();
    //GDALRegister_WMS();

    m_driversLoaded = true;
}

int DataStore::createMetadataTable()
{
    OGRLayer* pMetadataLayer = m_DS->CreateLayer(METHADATA_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pMetadataLayer) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }

    OGRFieldDefn oFieldKey(META_KEY, OFTString);
    oFieldKey.SetWidth(META_KEY_LIMIT);
    OGRFieldDefn oFieldValue(META_VALUE, OFTString);
    oFieldValue.SetWidth(META_VALUE_LIMIT);

    if(pMetadataLayer->CreateField(&oFieldKey) != OGRERR_NONE ||
       pMetadataLayer->CreateField(&oFieldValue) != OGRERR_NONE) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }
    
    FeaturePtr feature( OGRFeature::CreateFeature(
                pMetadataLayer->GetLayerDefn()) );

    // write version
    feature->SetField(META_KEY, NGS_VERSION_KEY);
    feature->SetField(META_VALUE, NGS_VERSION);
    if(pMetadataLayer->CreateFeature(feature) != OGRERR_NONE) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }
    
    return ngsErrorCodes::SUCCESS;
}

int DataStore::createRastersTable()
{
    OGRLayer* pRasterLayer = m_DS->CreateLayer(RASTERS_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pRasterLayer) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
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
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }

    return ngsErrorCodes::SUCCESS;
}

int DataStore::createAttachmentsTable()
{
    OGRLayer* pAttachmentsLayer = m_DS->CreateLayer(ATTACHEMENTS_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pAttachmentsLayer) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
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
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }

    return ngsErrorCodes::SUCCESS;
}

int DataStore::createMapsTable()
{
    OGRLayer* pMapsLayer = m_DS->CreateLayer(MAPS_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pMapsLayer) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }

    OGRFieldDefn oName(MAP_NAME, OFTString);
    oName.SetWidth(NAME_FIELD_LIMIT);
    OGRFieldDefn oEPSG(MAP_EPSG, OFTInteger);
    OGRFieldDefn oLayers(MAP_LAYERS, OFTInteger64List);
    OGRFieldDefn oMinX(MAP_MIN_X, OFTReal);
    OGRFieldDefn oMinY(MAP_MIN_Y, OFTReal);
    OGRFieldDefn oMaxX(MAP_MAX_X, OFTReal);
    OGRFieldDefn oMaxY(MAP_MAX_Y, OFTReal);
    OGRFieldDefn oDescription(MAP_DESCRIPTION, OFTString);
    oDescription.SetWidth(DESCRIPTION_FIELD_LIMIT);

    if(pMapsLayer->CreateField(&oName) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oEPSG) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oLayers) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oMinX) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oMinY) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oMaxX) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oMaxY) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oDescription) != OGRERR_NONE) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }

    return ngsErrorCodes::SUCCESS;
}

int DataStore::createLayersTable()
{
    OGRLayer* pMapsLayer = m_DS->CreateLayer(LAYERS_TABLE_NAME, NULL,
                                                   wkbNone, NULL);
    if (NULL == pMapsLayer) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }

    OGRFieldDefn oName(LAYER_NAME, OFTString);
    oName.SetWidth(NAME_FIELD_LIMIT);
    OGRFieldDefn oMapId(MAP_ID, OFTInteger64);
    OGRFieldDefn oType(LAYER_TYPE, OFTInteger);
    OGRFieldDefn oStyle(LAYER_STYLE, OFTString);
    OGRFieldDefn oDSName(DATASET_NAME, OFTString);
    oDSName.SetWidth(NAME_FIELD_LIMIT);

    if(pMapsLayer->CreateField(&oName) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oMapId) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oType) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oDSName) != OGRERR_NONE ||
       pMapsLayer->CreateField(&oStyle) != OGRERR_NONE) {
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }

    return ngsErrorCodes::SUCCESS;
}

int DataStore::upgrade(int /* oldVersion */)
{
    // no structure changes for version 1    
    return ngsErrorCodes::SUCCESS;
}

int DataStore::destroyDataset(Dataset::Type type, const string &name)
{
    // TODO: notify listeneres about changes
    switch (type) {
    case Dataset::REMOTE_TMS:
    case Dataset::NGW_IMAGE:
        CPLErrorReset();
        {
        string statement("DELETE FROM " RASTERS_TABLE_NAME " WHERE "
                         LAYER_NAME " = '");
        statement += name + "'";
        executeSQL ( statement );
        }
        if (CPLGetLastErrorNo() == CE_None)
            return ngsErrorCodes::SUCCESS;
        return ngsErrorCodes::DELETE_FAILED;

    case Dataset::LOCAL_RASTER:
    case Dataset::LOCAL_TMS:
        for (int i = 0; i < m_DS->GetLayerCount(); ++i)
        {
            OGRLayer* poLayer = m_DS->GetLayer(i);
            if(EQUAL(poLayer->GetName(), name.c_str ()))
                return m_DS->DeleteLayer (i) == OGRERR_NONE ?
                            ngsErrorCodes::SUCCESS :
                            ngsErrorCodes::DELETE_FAILED;
        }
    }

    return ngsErrorCodes::DELETE_FAILED;
}

void DataStore::notifyDatasetCanged(DataStore::ChangeType /*changeType*/,
                                    const string &/*name*/, long /*id*/)
{
    // TODO: notify listeners
}

bool DataStore::isNameValid(const string &name) const
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
    if(m_DS->GetLayerByName (name.c_str ()) != nullptr)
        return false;

    string statement("SELECT count(*) FROM " RASTERS_TABLE_NAME " WHERE "
                     LAYER_NAME " = '");
    statement += name + "'";
    ResultSetPtr tmpLayer = executeSQL ( statement );
    FeaturePtr feature = tmpLayer->GetFeature (0);
    if(feature->GetFieldAsInteger (0) > 0)
        return false;
    return true;
}

ResultSetPtr DataStore::executeSQL(const string &statement) const
{
    return ResultSetPtr(
                m_DS->ExecuteSQL ( statement.c_str(), nullptr, "SQLITE" ));
}

void DataStore::onLowMemory()
{
    m_datasources.clear ();
}
