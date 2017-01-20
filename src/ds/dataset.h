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
#ifndef NGSDATASET_H
#define NGSDATASET_H

#include "api_priv.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include <memory>
#include "ogrsf_frmts.h"

namespace ngs {

typedef std::shared_ptr< OGRLayer > ResultSetPtr;

/**
 * @brief The GDALDatasetPtr class Wrapper class for GDALDataset
 */
class GDALDatasetPtr : public std::shared_ptr< GDALDataset >
{
public:
    GDALDatasetPtr(GDALDataset* ds);
    GDALDatasetPtr();
    GDALDatasetPtr(const GDALDatasetPtr& ds);
    virtual ~GDALDatasetPtr();
    GDALDatasetPtr& operator=(GDALDataset* ds);
    operator GDALDataset*() const;
};

class Dataset;
typedef std::shared_ptr<Dataset> DatasetPtr;
// typedef weak_ptr<Dataset> DatasetWPtr;
#define ngsDatasetType(x) static_cast<unsigned int>( ngs::Dataset::Type::x )

class ProgressInfo
{
public:
    ProgressInfo(unsigned int id, const char **options = nullptr,
                 ngsProgressFunc progressFunc = nullptr,
                 void *progressArguments = nullptr);
    virtual ~ProgressInfo();
    ProgressInfo(const ProgressInfo& data);
    ProgressInfo& operator=(const ProgressInfo& data);
    unsigned int id() const;
    char **options() const;
    bool onProgress(double complete, const char* message) const;
    ngsErrorCodes status() const;
    void setStatus(const ngsErrorCodes &status);
protected:
    unsigned int m_id;
    char** m_options;
    ngsProgressFunc m_progressFunc;
    void *m_progressArguments;
    enum ngsErrorCodes m_status;
};

/**
 * @brief The Dataset class is base class of DataStore. Each table, raster,
 * feature class, etc. are Dataset. The DataStore is an array of Datasets as
 * Map is array of Layers.
 */
class Dataset
{
    friend class DatasetContainer;
    friend class DataStore;
public:
    enum class Type {
        Undefined   = 1 << 0,
        Container   = 1 << 1,
        Table       = 1 << 2,
        Featureset  = 1 << 3,
        Raster      = 1 << 4,
        Store       = 1 << 5
    };

    enum class ChangeType {
        AddFeature = ngsChangeCodes::CC_CREATE_FEATURE,
        ChangeFeature = ngsChangeCodes::CC_CHANGE_FEATURE,
        DeleteFeature = ngsChangeCodes::CC_DELETE_FEATURE,
        DeleteAllFeatures = ngsChangeCodes::CC_DELETEALL_FEATURES,
        AddAttachment = ngsChangeCodes::CC_CREATE_ATTACHMENT,
        ChangeAttachment = ngsChangeCodes::CC_CHANGE_ATTACHMENT,
        DeleteAttachment = ngsChangeCodes::CC_DELETE_ATTACHMENT,
        DeleteAllAttachments = ngsChangeCodes::CC_DELETEALL_ATTACHMENTS,
        AddDataset,
        ChangeDataset,
        DeleteDataset
    };
public:
    Dataset();
    virtual ~Dataset();

    // base properties
    unsigned int type() const;
    bool isVector() const;
    bool isRaster() const;

    CPLString name() const;
    CPLString path() const;

    // is checks
    bool isDeleted() const;
    bool isOpened() const;
    bool isReadOnly() const;

    // can checks
    bool canDelete(void);
    bool canRename(void);

    // base operations
    virtual int destroy(ProgressInfo *processInfo = nullptr);
    /* TODO: release this
    int rename(const CPLString &newName, ngsProgressFunc progressFunc = nullptr,
               void* progressArguments = nullptr);
               */
    // static functions
    static DatasetPtr create(const CPLString& path, const CPLString& driver,
                             char **options = nullptr);
    static DatasetPtr open(const CPLString& path,  unsigned int openFlags,
                           char **options = nullptr);

    // notifications
    void setNotifyFunc(ngsNotifyFunc notifyFunc);
    void unsetNotifyFunc();
protected:
    virtual void setName(const CPLString& path);
    void notifyDatasetChanged(enum ChangeType changeType,
                              const CPLString &name, long id);
    static DatasetPtr getDatasetForGDAL(const CPLString& path, GDALDatasetPtr ds);
    inline int reportError(int code, double percent,
                           const char* message, ProgressInfo *processInfo = nullptr) {
        //CPLError(CE_Failure, CPLE_AppDefined, message);
        CPLErrorSetState(CE_Failure, CPLE_AppDefined, message);
        if(processInfo) {
            processInfo->setStatus (static_cast<ngsErrorCodes>(code));
            processInfo->onProgress (percent, message);
        }
        return code;
    }

protected:
    unsigned int m_type;
    CPLString m_name, m_path;
    bool m_deleted;
    bool m_opened;
    bool m_readOnly;
    GDALDatasetPtr m_DS;
    ngsNotifyFunc m_notifyFunc;
};

}

#endif // NGSDATASET_H
