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
#ifndef DATASET_H
#define DATASET_H

#include "api.h"

#include "cpl_string.h"
#include "ogrsf_frmts.h"
#include "gdal_frmts.h"

#include <memory>

namespace ngs {

using namespace std;

typedef shared_ptr< OGRLayer > ResultSetPtr;

/**
 * @brief The GDALDatasetPtr class Wrapper class for GDALDataset
 */
class GDALDatasetPtr : public shared_ptr< GDALDataset >
{
public:
    GDALDatasetPtr(GDALDataset* ds);
    GDALDatasetPtr();
    GDALDatasetPtr& operator=(GDALDataset* pFeature);
    operator GDALDataset*() const;
};

class Dataset;
typedef shared_ptr<Dataset> DatasetPtr;
typedef weak_ptr<Dataset> DatasetWPtr;

/**
 * @brief The Dataset class is base class of DataStore. Each table, raster,
 * feature class, etc. are Dataset. The DataStore is an array of Datasets as
 * Map is array of Layers.
 */
class Dataset
{
    friend class DatasetContainer;
public:
    enum Type {
        UNDEFINED,
        CONTAINER,
        TABLE,
        FEATURESET,
        RASTER
    };

    enum ChangeType {
        ADD_FEATURE = ngsChangeCodes::CREATE_FEATURE,
        CHANGE_FEATURE = ngsChangeCodes::CHANGE_FEATURE,
        DELETE_FEATURE = ngsChangeCodes::DELETE_FEATURE,
        DELETEALL_FEATURES = ngsChangeCodes::DELETEALL_FEATURES,
        ADD_ATTACHMENT = ngsChangeCodes::CREATE_ATTACHMENT,
        CHANGE_ATTACHMENT = ngsChangeCodes::CHANGE_ATTACHMENT,
        DELETE_ATTACHMENT = ngsChangeCodes::DELETE_ATTACHMENT,
        DELETEALL_ATTACHMENTS = ngsChangeCodes::DELETEALL_ATTACHMENTS,
        ADD_DATASET,
        CHANGE_DATASET,
        DELETE_DATASET
    };
public:
    Dataset();
    virtual ~Dataset();

    // base properties
    Type type() const;
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
    virtual int destroy(ngsProgressFunc progressFunc = nullptr,
                void* progressArguments = nullptr);
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

protected:
    enum Type m_type;
    CPLString m_name, m_path;
    bool m_deleted;
    bool m_opened;
    bool m_readOnly;
    GDALDatasetPtr m_DS;
    ngsNotifyFunc m_notifyFunc;
};

}

#endif // DATASET_H
