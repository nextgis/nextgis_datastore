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
#include "api.h"
#include "rasterdataset.h"

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
#define MAIN_DATABASE "ngs.gpkg"
#define METHADATA_TABLE_NAME "ngs_meta"
#define ATTACHEMENTS_TABLE_NAME "ngs_attach"
#define RASTER_LAYER_TABLE_NAME "ngs_raster"
#define SYS_TABLE_COUNT 2
#define META_KEY "key"
#define META_KEY_LIMIT 64
#define META_VALUE "value"
#define META_VALUE_LIMIT 255
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
#define NGS_VERSION_KEY "ngs_version"
#define NGS_VERSION 1
#define NAME_FIELD_LIMIT 64
#define ALIAS_FIELD_LIMIT 255

#define HTTP_TIMEOUT "5"
#define HTTP_USE_GZIP "ON"

using namespace ngs;

//------------------------------------------------------------------------------
// OGRFeaturePtr
//------------------------------------------------------------------------------
OGRFeaturePtr::OGRFeaturePtr(OGRFeature* pFeature) :
    unique_ptr(pFeature, OGRFeature::DestroyFeature )
{

}

OGRFeaturePtr:: OGRFeaturePtr() :
    unique_ptr(nullptr, OGRFeature::DestroyFeature )
{

}

OGRFeaturePtr::OGRFeaturePtr( OGRFeaturePtr& other ) :
    unique_ptr< OGRFeature, void (*)(OGRFeature*) >( move( other ) )
{

}

OGRFeaturePtr& OGRFeaturePtr::operator=(OGRFeaturePtr& feature) {
    move(feature);
    return *this;
}

OGRFeaturePtr& OGRFeaturePtr::operator=(OGRFeature* pFeature) {
    reset(pFeature);
    return *this;
}

OGRFeaturePtr::operator OGRFeature *() const
{
    return get();
}

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
        return ngsErrorCodes::UNSUPPORTED_GDAL_DRIVER;
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
        return ngsErrorCodes::UNSUPPORTED_GDAL_DRIVER;
    }

    m_DS = static_cast<GDALDataset*>( GDALOpenEx(sFullPath.c_str (),
                                                   GDAL_OF_SHARED|GDAL_OF_UPDATE,
                                                   nullptr, nullptr, nullptr) );

    // check version and upgrade if needed
    OGRLayer* pMetadataLayer = m_DS->GetLayerByName (METHADATA_TABLE_NAME);
    if(nullptr == pMetadataLayer)
        return ngsErrorCodes::INVALID_DB_STUCTURE;

    pMetadataLayer->ResetReading();
    OGRFeaturePtr feature;
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

    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTER_LAYER_TABLE_NAME);
    if(nullptr == pRasterLayer)
        return ngsErrorCodes::INVALID_DB_STUCTURE;

    // TODO: fill m_datasources

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

    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTER_LAYER_TABLE_NAME);
    OGRFeaturePtr feature( OGRFeature::CreateFeature(
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

    // TODO: add to m_datasources

    return ngsErrorCodes::SUCCESS;
}

int DataStore::datasetCount() const
{
    int dsLayerCount = m_DS->GetLayerCount ();
    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTER_LAYER_TABLE_NAME);
    dsLayerCount += pRasterLayer->GetFeatureCount ();
    return dsLayerCount - SYS_TABLE_COUNT;
}

Dataset *DataStore::getDataset(const char *name)
{
    auto it = m_datasources.find (name);
    if( it != m_datasources.end ()){
        if(!it->second->deleted ()){
            return it->second.get();
        }
        else{
            return nullptr;
        }
    }

    Dataset *pDataset = nullptr;
    OGRLayer* pLayer = m_DS->GetLayerByName (name);
    if(nullptr != pLayer){
        return nullptr; // TODO: create Dataset* and add it to map
    }

    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTER_LAYER_TABLE_NAME);
    OGRFeaturePtr feature;
    while( (feature = pRasterLayer->GetNextFeature()) != nullptr ) {
        if(EQUAL(feature->GetFieldAsString (LAYER_NAME), name)){

            switch(feature->GetFieldAsInteger (LAYER_TYPE)){
            case Dataset::REMOTE_TMS:
            {
                RemoteTMSDataset* pRemoteTMSDataset =
                        new RemoteTMSDataset(this,
                                        feature->GetFieldAsString (LAYER_NAME),
                                        feature->GetFieldAsString (LAYER_ALIAS));
                pDataset = pRemoteTMSDataset;
                m_datasources[pRemoteTMSDataset->name ()].reset (pDataset);
                break;
            }
            }

            return pDataset; // TODO: create Dataset* and add it to map
        }
    }
    return pDataset;
}

Dataset *DataStore::getDataset(int index)
{
    int dsLayers = m_DS->GetLayerCount () - SYS_TABLE_COUNT;
    if(index < dsLayers){
        int counter = 0;
        for(int i = 0; i < m_DS->GetLayerCount (); ++i){
            OGRLayer* pLayer = m_DS->GetLayer (i);
            // skip system tables
            if(EQUAL (pLayer->GetName (), METHADATA_TABLE_NAME))
                continue;
            else if(EQUAL (pLayer->GetName (), ATTACHEMENTS_TABLE_NAME))
                continue;
            else if(EQUAL (pLayer->GetName (), RASTER_LAYER_TABLE_NAME))
                continue;
            if(counter == index)
                return getDataset (pLayer->GetName());
            counter++;
        }
    }
    OGRLayer* pRasterLayer = m_DS->GetLayerByName (RASTER_LAYER_TABLE_NAME);
    OGRFeaturePtr feature( pRasterLayer->GetFeature (index - dsLayers) );
    return getDataset (feature->GetFieldAsString (LAYER_NAME));
}

string &DataStore::formats()
{
    if(m_formats.empty ()){
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

            m_formats += CPLSPrintf( "  %s -%s- (%s%s%s%s): %s\n",
                    GDALGetDriverShortName( hDriver ),
                    pszKind,
                    pszRFlag, pszWFlag, pszVirtualIO, pszSubdatasets,
                    GDALGetDriverLongName( hDriver ) );
        }
    }
    return m_formats;
}

void DataStore::initGDAL()
{
    // set config options
    CPLSetConfigOption("GDAL_DATA", m_dataPath.c_str());
    CPLSetConfigOption("GDAL_HTTP_USERAGENT", NGM_USERAGENT);
    CPLSetConfigOption("CPL_CURL_GZIP", HTTP_USE_GZIP);
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", HTTP_TIMEOUT);
    CPLSetConfigOption("CPL_TMPDIR", m_cachePath.c_str());

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
    
    OGRFeaturePtr feature( OGRFeature::CreateFeature(
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
    OGRLayer* pRasterLayer = m_DS->CreateLayer(RASTER_LAYER_TABLE_NAME, NULL,
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
    return ngsErrorCodes::SUCCESS;
}

int DataStore::upgrade(int /* oldVersion */)
{
    // no structure changes for version 1    
    return ngsErrorCodes::SUCCESS;
}

int DataStore::destroyDaraset(Dataset::Type type, const string &name)
{
    switch (type) {
    case Dataset::REMOTE_TMS:
    case Dataset::NGW_IMAGE:
        CPLErrorReset();
        {
        string statement("DELETE FROM " RASTER_LAYER_TABLE_NAME " WHERE "
                         LAYER_NAME " = '");
        statement += name + "'";
        m_DS->ExecuteSQL ( statement.c_str(), nullptr, nullptr );
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

bool DataStore::isNameValid(const string &name) const
{
    if(name.size () < 4)
        return false;
    if((name[0] == 'n' || name[0] == 'N') &&
       (name[1] == 'g' || name[1] == 'G') &&
       (name[2] == 's' || name[2] == 'S') &&
       (name[3] == '_'))
        return false;
    if(m_datasources.find (name) != m_datasources.end ())
        return false;
    return true;
}
