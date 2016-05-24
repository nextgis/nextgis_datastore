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
#include "cpl_vsi.h"

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
#define META_KEY "key"
#define META_KEY_LIMIT 64
#define META_VALUE "value"
#define META_VALUE_LIMIT 255
#define NGS_VERSION_KEY "ngs_version"
#define NGS_VERSION 1

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

//------------------------------------------------------------------------------
// DataStore
//------------------------------------------------------------------------------

DataStore::DataStore(const char* path, const char* dataPath,
                     const char* cachePath) : m_poDS(nullptr)
{
    if(nullptr != path)
        m_sPath = path;
    if(nullptr == cachePath) {
        if(nullptr != path) {
            m_sCachePath = path;
            m_sCachePath += DEFAULT_CACHE;
        }
    }
    else {
        m_sCachePath = cachePath;
    }
    if(nullptr == dataPath) {
        if(nullptr != path) {
            m_sDataPath = path;
            m_sDataPath += DEFAULT_DATA;
        }
    }
    else {
        m_sDataPath = dataPath;
    }

    m_bDriversLoaded = false;
}

DataStore::~DataStore()
{
    GDALClose( m_poDS );

    GDALDestroyDriverManager();
}

int DataStore::create()
{
    if(nullptr != m_poDS)
        return ngsErrorCodes::UNEXPECTED_ERROR;

    if(m_sPath.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;

    registerDrivers();
    // get GeoPackage driver
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
    if( poDriver == nullptr ) {
        return ngsErrorCodes::UNSUPPORTED_GDAL_DRIVER;
    }

    // create folder if not exist
    if( VSIMkdir( m_sPath.c_str (), 0755 ) != 0 ) {
        return ngsErrorCodes::CREATE_DIR_FAILED;
    }

    // create folder if not exist
    if( VSIMkdir( m_sCachePath.c_str (), 0755 ) != 0 ) {
        return ngsErrorCodes::CREATE_DIR_FAILED;
    }

    string sFullPath = m_sPath + string(PATH_SEPARATOR MAIN_DATABASE);

    // create ngm.gpkg
    m_poDS = poDriver->Create( sFullPath.c_str (), 0, 0, 0,
                                          GDT_Unknown, nullptr );
    if( m_poDS == nullptr ) {
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
    if(m_sPath.empty())
        return ngsErrorCodes::PATH_NOT_SPECIFIED;
    string sFullPath = m_sPath + string(PATH_SEPARATOR MAIN_DATABASE);
    if (CPLCheckForFile(const_cast<char*>(sFullPath.c_str()), nullptr) != TRUE) {
        return ngsErrorCodes::INVALID_PATH;
    }

    registerDrivers();
    // get GeoPackage driver
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName("GPKG");
    if( poDriver == nullptr ) {
        return ngsErrorCodes::UNSUPPORTED_GDAL_DRIVER;
    }

    m_poDS = static_cast<GDALDataset*>( GDALOpenEx(sFullPath.c_str (),
                                                   GDAL_OF_UPDATE,
                                                   nullptr, nullptr, nullptr) );

    // check version and upgrade if needed
    OGRLayer* pMetadataLayer = m_poDS->GetLayerByName (METHADATA_TABLE_NAME);
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

    return ngsErrorCodes::SUCCESS;
}

int DataStore::openOrCreate()
{
    if(open() != ngsErrorCodes::SUCCESS)
        return create();
    return open();
}

string &DataStore::reportFormats()
{
    if(m_sFormats.empty ()){
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

            m_sFormats += CPLSPrintf( "  %s -%s- (%s%s%s%s): %s\n",
                    GDALGetDriverShortName( hDriver ),
                    pszKind,
                    pszRFlag, pszWFlag, pszVirtualIO, pszSubdatasets,
                    GDALGetDriverLongName( hDriver ) );
        }
    }
    return m_sFormats;
}

int DataStore::destroy()
{
    if(m_sPath.empty())
        return ngsErrorCodes::INVALID_PATH;
    if(!m_sCachePath.empty()) {
        if(CPLUnlinkTree(m_sCachePath.c_str()) != 0){
            return ngsErrorCodes::DELETE_FAILED;
        }
    }
    return CPLUnlinkTree (m_sPath.c_str()) == 0 ? ngsErrorCodes::SUCCESS :
                                                  ngsErrorCodes::DELETE_FAILED;
}

void DataStore::registerDrivers()
{
    if(m_bDriversLoaded)
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

    m_bDriversLoaded = true;
}

int DataStore::createMetadataTable()
{
    OGRLayer* pMetadataLayer = m_poDS->CreateLayer(METHADATA_TABLE_NAME, NULL, 
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
    
    OGRFeature *poFeature = OGRFeature::CreateFeature(
                pMetadataLayer->GetLayerDefn());

    // write version
    poFeature->SetField(META_KEY, NGS_VERSION_KEY);
    poFeature->SetField(META_VALUE, NGS_VERSION);
    if(pMetadataLayer->CreateFeature(poFeature) != OGRERR_NONE) {
        OGRFeature::DestroyFeature( poFeature );
        return ngsErrorCodes::CREATE_TABLE_FAILED;
    }
    OGRFeature::DestroyFeature(poFeature);
    
    return ngsErrorCodes::SUCCESS;
}

int DataStore::createRastersTable()
{
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
