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
#include "api.h"
#include "dataset.h"
#include "datasetcontainer.h"
#include "featuredataset.h"
#include "rasterdataset.h"
#include "stringutil.h"

#include <iostream>

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

GDALDatasetPtr::GDALDatasetPtr(const GDALDatasetPtr &ds) : shared_ptr(ds)
{
}

GDALDatasetPtr::~GDALDatasetPtr()
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
// ProgressInfo
//------------------------------------------------------------------------------
ProgressInfo::ProgressInfo(unsigned int id, const char **options,
                           ngsProgressFunc progressFunc, void *progressArguments) :
    m_id(id), m_options(CSLDuplicate (const_cast<char**>(options))),
    m_progressFunc(progressFunc),
    m_progressArguments(progressArguments),
    m_status(ngsErrorCodes::EC_PENDING)
{

}

ProgressInfo::~ProgressInfo()
{
    CSLDestroy (m_options);
}

ProgressInfo::ProgressInfo(const ProgressInfo &data)
{
    m_id = data.m_id;
    m_options = CSLDuplicate (data.m_options);
    m_progressFunc = data.m_progressFunc;
    m_progressArguments = data.m_progressArguments;
    m_status = data.m_status;
}

ProgressInfo &ProgressInfo::operator=(const ProgressInfo &data)
{
    m_id = data.m_id;
    m_options = CSLDuplicate (data.m_options);
    m_progressFunc = data.m_progressFunc;
    m_progressArguments = data.m_progressArguments;
    m_status = data.m_status;
    return *this;
}

ngsErrorCodes ProgressInfo::status() const
{
    return m_status;
}

void ProgressInfo::setStatus(const ngsErrorCodes &status)
{
    m_status = status;
}

bool ProgressInfo::onProgress(double complete, const char *message) const
{
    if(nullptr == m_progressFunc)
        return true; // no cancel from user
    return m_progressFunc(m_id, complete, message, m_progressArguments) == TRUE;
}


char **ProgressInfo::options() const
{
    return m_options;
}

unsigned int ProgressInfo::id() const
{
    return m_id;
}

//------------------------------------------------------------------------------
// Dataset
//------------------------------------------------------------------------------

Dataset::Dataset() : m_deleted(false), m_opened(false), m_readOnly(true),
    m_notifyFunc(nullptr)
{
    m_type = ngsDatasetType(Undefined);
}

Dataset::~Dataset()
{

}

unsigned int Dataset::type() const
{
    return m_type;
}

bool Dataset::isVector() const
{
    return m_type & ngsDatasetType(Table) || m_type & ngsDatasetType(Featureset);
}

bool Dataset::isRaster() const
{
    return m_type & ngsDatasetType(Raster);
}

CPLString Dataset::name() const
{
    return m_name;
}

int Dataset::destroy(ProgressInfo *processInfo)
{
    m_DS.reset ();
    int nRes = CPLUnlinkTree (m_path) == 0 ? ngsErrorCodes::EC_SUCCESS :
                                             ngsErrorCodes::EC_DELETE_FAILED;
    if(nRes == ngsErrorCodes::EC_SUCCESS)
        m_deleted = true;
    if(processInfo)
        processInfo->setStatus(static_cast<ngsErrorCodes>(nRes));
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
    Dataset* outDS = nullptr;
    if((testBoolean(driver->GetMetadataItem (GDAL_DMD_SUBDATASETS), false) &&
       !testBoolean(driver->GetMetadataItem (GDAL_DCAP_RASTER), false) ) ||
            ds->GetLayerCount () > 1) {
        outDS = new DatasetContainer();
    }
    else if (testBoolean(driver->GetMetadataItem (GDAL_DCAP_RASTER), false)) {
        RasterDataset* raster = new RasterDataset();
        raster->m_spatialReference.SetFromUserInput (ds->GetProjectionRef ());
        outDS = raster;
    }
    else { //if (testBoolean(driver->GetMetadataItem (GDAL_DCAP_VECTOR), false))
        OGRLayer* layer = ds->GetLayer (0);
        if(nullptr != layer){
            OGRFeatureDefn* defn = layer->GetLayerDefn ();
            if( defn->GetGeomFieldCount () == 0 ) {
                outDS = new Table(layer);
            }
            else {
                outDS = new FeatureDataset(layer);
            }       
        }
    }

    if (!outDS) {
        return nullptr;
    }

    outDS->m_DS = ds;
    outDS->m_path = path;
    outDS->m_opened = true;

    out.reset (outDS);
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
        m_notifyFunc(ngsSourceCodes::SC_DATASET, name,
                     id, static_cast<ngsChangeCodes>(changeType));
    }
}
