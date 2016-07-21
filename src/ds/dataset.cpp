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
#include "datasetcontainer.h"
#include "rasterdataset.h"
#include "featuredataset.h"
#include "api.h"

using namespace ngs;

//------------------------------------------------------------------------------
// GDALDatasetPtr
//------------------------------------------------------------------------------

GDALDatasetPtr::GDALDatasetPtr(GDALDataset* ds) :
    shared_ptr(ds, GDALClose )
{

}

GDALDatasetPtr::GDALDatasetPtr() :
    shared_ptr(nullptr, GDALClose )
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

Dataset::Dataset() : m_deleted(false), m_opened(false), m_readOnly(true),
    m_notifyFunc(nullptr)
{
    m_type = UNDEFINED;
}

Dataset::~Dataset()
{

}

Dataset::Type Dataset::type() const
{
    return m_type;
}

bool Dataset::isVector() const
{
    return m_type == TABLE || m_type == FEATURESET;
}

bool Dataset::isRaster() const
{
    return m_type == RASTER;
}

CPLString Dataset::name() const
{
    return m_name;
}

int Dataset::destroy(ngsProgressFunc /*progressFunc*/, void* /*progressArguments*/)
{
    m_DS.reset ();
    int nRes = CPLUnlinkTree (m_path) == 0 ? ngsErrorCodes::SUCCESS :
                                             ngsErrorCodes::DELETE_FAILED;
    if(nRes == ngsErrorCodes::SUCCESS)
        m_deleted = true;
    return nRes;
}

bool Dataset::isDeleted() const
{
    return m_deleted;
}

CPLString Dataset::path() const
{
    return m_path;
}

bool Dataset::isOpened() const
{
    return m_opened;
}

bool Dataset::isReadOnly() const
{
    return m_readOnly;
}

bool Dataset::canDelete()
{
    // TODO: correct this
    return true;
}

bool Dataset::canRename()
{
    // TODO: correct this
    return true;
}

DatasetPtr Dataset::create(const CPLString& path, const CPLString& driver,
                           char **options)
{
    DatasetPtr out;
    if(path.empty())
        return out;

    // get GeoPackage driver
    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(driver);
    if( poDriver == nullptr )
        return out;

    // create
    CPLErrorReset();
    GDALDatasetPtr DS = poDriver->Create( path, 0, 0, 0, GDT_Unknown, options );
    if( DS == nullptr ) {
        return out;
    }

    return getDatasetForGDAL(path, DS);
}

DatasetPtr Dataset::getDatasetForGDAL(const CPLString& path, GDALDatasetPtr ds)
{
    GDALDriver* driver = ds->GetDriver ();
    DatasetPtr out;
    Dataset* pOutDS = nullptr;
    if(CPLTestBoolean(driver->GetMetadataItem (GDAL_DMD_SUBDATASETS, nullptr)) ||
            ds->GetLayerCount () > 1) {
        pOutDS = new DatasetContainer();
    }
    else if (CPLTestBoolean(driver->GetMetadataItem (GDAL_DCAP_RASTER, nullptr))) {
        pOutDS = new RasterDataset();
    }
    else {

        OGRLayer* pLayer = ds->GetLayer (0);
        if(nullptr != pLayer){
            if( EQUAL(pLayer->GetGeometryColumn (), "")){
                pOutDS = new Table(pLayer);
            }
            else {
                pOutDS = new FeatureDataset(pLayer);
            }

        //if (CPLTestBoolean(driver->GetMetadataItem (GDAL_DCAP_VECTOR, nullptr)))
        }
    }

    pOutDS->m_DS = ds;
    pOutDS->m_path = path;
    pOutDS->m_opened = true;

    out.reset (pOutDS);
    return out;
}

DatasetPtr Dataset::open(const CPLString &path, unsigned int openFlags,
                         char **options)
{
    DatasetPtr out;
    if(nullptr == path)
        return out;
    // open
    CPLErrorReset();
    GDALDatasetPtr DS = static_cast<GDALDataset*>( GDALOpenEx( path,  openFlags,
                                                    nullptr, options, nullptr));
    if( DS == nullptr ) {
        return out;
    }

    return getDatasetForGDAL(path, DS);
}

void Dataset::setNotifyFunc(ngsNotifyFunc notifyFunc)
{
    m_notifyFunc = notifyFunc;
}

void Dataset::unsetNotifyFunc()
{
    m_notifyFunc = nullptr;
}

void Dataset::setName(const CPLString &path)
{
    m_name = CPLGetBasename (path);
}

void Dataset::notifyDatasetChanged(enum ChangeType changeType,
                                    const CPLString &name, long id)
{
    // notify listeners
    if(nullptr != m_notifyFunc) {
        m_notifyFunc(ngsSourceCodes::DATASET, name,
                     id, static_cast<ngsChangeCodes>(changeType));
    }
}
