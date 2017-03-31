/******************************************************************************
*  Project: NextGIS ...
*  Purpose: Application for ...
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2012-2016 NextGIS, info@nextgis.ru
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "dataset.h"

#include <iostream>

//#include "datasetcontainer.h"
//#include "featuredataset.h"
//#include "rasterdataset.h"
#include "ngstore/api.h"
#include "catalog/folder.h"
#include "util/error.h"
#include "util/stringutil.h"

namespace ngs {

//------------------------------------------------------------------------------
// GDALDatasetPtr
//------------------------------------------------------------------------------

GDALDatasetPtr::GDALDatasetPtr(GDALDataset* ds) :
    shared_ptr(ds, GDALClose)
{
}

GDALDatasetPtr::GDALDatasetPtr() :
    shared_ptr(nullptr, GDALClose)
{

}

GDALDatasetPtr::GDALDatasetPtr(const GDALDatasetPtr &ds) : shared_ptr(ds)
{
}

GDALDatasetPtr& GDALDatasetPtr::operator=(GDALDataset* ds) {
    reset(ds);
    return *this;
}

GDALDatasetPtr::operator GDALDataset *() const
{
    return get();
}

//------------------------------------------------------------------------------
// Dataset
//------------------------------------------------------------------------------

Dataset::Dataset(ObjectContainer * const parent,
                 const ngsCatalogObjectType type,
                 const CPLString &name,
                 const CPLString &path) : Object(parent, type, name, path),
    m_DS(nullptr)
{
}

Dataset::~Dataset()
{
    GDALClose(m_DS);
}

bool Dataset::open(unsigned int openFlags, char **options)
{
    if(nullptr == m_path || EQUAL(m_path, "")) {
        return errorMessage(ngsErrorCodes::EC_OPEN_FAILED, _("The path is empty"));
    }

    // open
    CPLErrorReset();
    m_DS = static_cast<GDALDataset*>(GDALOpenEx( m_path, openFlags, nullptr,
                                                 options, nullptr));
    if(m_DS == nullptr) {
        return false; // Error message comes from GDALOpenEx
    }

    return true;
}

bool Dataset::destroy()
{
    GDALClose(m_DS);
    m_DS = nullptr;
    return Folder::deleteFile(m_path);
}

//int Dataset::destroy(ProgressInfo *processInfo)
//{
//    m_DS.reset ();
//    int nRes = CPLUnlinkTree (m_path) == 0 ? ngsErrorCodes::EC_SUCCESS :
//                                             ngsErrorCodes::EC_DELETE_FAILED;
//    if(nRes == ngsErrorCodes::EC_SUCCESS)
//        m_deleted = true;
//    if(processInfo)
//        processInfo->setStatus(static_cast<ngsErrorCodes>(nRes));
//    return nRes;
//}

//DatasetPtr Dataset::create(const char* path, const char* driver,
//                           char **options)
//{
//    DatasetPtr out;
//    if(nullptr == path || EQUAL(path, "")) {
//        CPLError(CE_Failure, CPLE_AppDefined, _("The path is empty"));
//        return out;
//    }

//    // get GeoPackage driver
//    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(driver);
//    if( poDriver == nullptr ) {
//        CPLError(CE_Failure, CPLE_AppDefined, _("The GDAL driver '%s' not found"),
//                 driver);
//        return out;
//    }

//    // create
//    CPLErrorReset();
//    GDALDatasetPtr DS = poDriver->Create( path, 0, 0, 0, GDT_Unknown, options );
//    if( DS == nullptr ) {
//        return out;
//    }

//    return getDatasetForGDAL(path, DS);
//}

//void Dataset::setName(const CPLString &path)
//{
//    m_name = CPLGetBasename (path);
//}


//DatasetPtr Dataset::getDatasetForGDAL(const CPLString& path, GDALDatasetPtr ds)
//{
//    GDALDriver* driver = ds->GetDriver ();

//    DatasetPtr out;
//    Dataset* outDS = nullptr;
//    if((testBoolean(driver->GetMetadataItem (GDAL_DMD_SUBDATASETS), false) &&
//       !testBoolean(driver->GetMetadataItem (GDAL_DCAP_RASTER), false) ) ||
//            ds->GetLayerCount () > 1) {
//        outDS = new DatasetContainer();
//    }
//    else if (testBoolean(driver->GetMetadataItem (GDAL_DCAP_RASTER), false)) {
//        RasterDataset* raster = new RasterDataset();
//        raster->m_spatialReference.SetFromUserInput (ds->GetProjectionRef ());
//        outDS = raster;
//    }
//    else { //if (testBoolean(driver->GetMetadataItem (GDAL_DCAP_VECTOR), false))
//        OGRLayer* layer = ds->GetLayer (0);
//        if(nullptr != layer){
//            OGRFeatureDefn* defn = layer->GetLayerDefn ();
//            if( defn->GetGeomFieldCount () == 0 ) {
//                outDS = new Table(layer);
//            }
//            else {
//                outDS = new FeatureDataset(layer);
//            }
//        }
//    }

//    if (!outDS) {
//        return nullptr;
//    }

//    outDS->m_DS = ds;
//    outDS->m_path = path;
//    outDS->m_opened = true;

//    out.reset (outDS);
//    return out;
//}


//------------------------------------------------------------------------------
// ProgressInfo
//------------------------------------------------------------------------------
//ProgressInfo::ProgressInfo(unsigned int id, const char **options,
//                           ngsProgressFunc progressFunc, void *progressArguments) :
//    m_id(id), m_options(CSLDuplicate (const_cast<char**>(options))),
//    m_progressFunc(progressFunc),
//    m_progressArguments(progressArguments),
//    m_status(ngsErrorCodes::EC_PENDING)
//{

//}

//ProgressInfo::~ProgressInfo()
//{
//    CSLDestroy (m_options);
//}

//ProgressInfo::ProgressInfo(const ProgressInfo &data)
//{
//    m_id = data.m_id;
//    m_options = CSLDuplicate (data.m_options);
//    m_progressFunc = data.m_progressFunc;
//    m_progressArguments = data.m_progressArguments;
//    m_status = data.m_status;
//}

//ProgressInfo &ProgressInfo::operator=(const ProgressInfo &data)
//{
//    m_id = data.m_id;
//    m_options = CSLDuplicate (data.m_options);
//    m_progressFunc = data.m_progressFunc;
//    m_progressArguments = data.m_progressArguments;
//    m_status = data.m_status;
//    return *this;
//}

//ngsErrorCodes ProgressInfo::status() const
//{
//    return m_status;
//}

//void ProgressInfo::setStatus(const ngsErrorCodes &status)
//{
//    m_status = status;
//}

//bool ProgressInfo::onProgress(double complete, const char *message) const
//{
//    if(nullptr == m_progressFunc)
//        return true; // no cancel from user
//    return m_progressFunc(ngsErrorCodes::EC_SUCCESS/*m_id*/, complete, message, m_progressArguments) == TRUE;
//}


//char **ProgressInfo::options() const
//{
//    return m_options;
//}

//unsigned int ProgressInfo::id() const
//{
//    return m_id;
//}



} // namespace ngs
