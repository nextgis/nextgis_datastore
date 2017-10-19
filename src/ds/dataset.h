/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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

#include <memory>

#include "api_priv.h"
#include "featureclass.h"
#include "geometry.h"

#include "catalog/objectcontainer.h"
#include "ngstore/codes.h"
#include "util/stringutil.h"

namespace ngs {

constexpr const char* OVR_ZOOM_KEY = "z";
constexpr const char* OVR_X_KEY = "x";
constexpr const char* OVR_Y_KEY = "y";
constexpr const char* OVR_TILE_KEY = "tile";
constexpr const char* ATTACH_FEATURE_ID_FIELD = "afid";
constexpr const char* ATTACH_FILE_NAME_FIELD = "name";
constexpr const char* ATTACH_DESCRIPTION_FIELD = "descript";
constexpr const char* FEATURE_ID_FIELD = "fid";
constexpr const char* OPERATION_FIELD = "op";

constexpr const char* USER_KEY = "user";
constexpr const char* NG_ADDITIONS_KEY = "nga";
constexpr const char* USER_PREFIX_KEY = "USER.";
constexpr int USER_PREFIX_KEY_LEN = length(USER_PREFIX_KEY);
constexpr const char* NGS_VERSION_KEY = "version";

constexpr const char* METHADATA_TABLE_NAME = "nga_meta";
// Overviews
constexpr const char* OVR_PREFIX = "overviews_";
constexpr int OVR_PREFIX_LEN = length(OVR_PREFIX);

/**
 * @brief The wrapper class around GDALDataset pointer
 */
class GDALDatasetPtr : public std::shared_ptr<GDALDataset>
{
public:
    explicit GDALDatasetPtr(GDALDataset* ds);
    GDALDatasetPtr();
    explicit GDALDatasetPtr(const GDALDatasetPtr& ds);
    GDALDatasetPtr& operator=(GDALDataset* ds);
    operator GDALDataset*() const;
};

/**
 * @brief The DatasetBase class
 */
class DatasetBase
{
public:
    DatasetBase();
    virtual ~DatasetBase();
    virtual const char* options(enum ngsOptionType optionType) const = 0;
    virtual GDALDataset *getGDALDataset() const { return m_DS; }
    virtual bool open(unsigned int openFlags, const Options &options = Options()) = 0;
    // is checks
    virtual bool isOpened() const { return m_DS != nullptr; }
protected:
    bool open(const char* path, unsigned int openFlags,
              const Options &options = Options());
    const char* options(const enum ngsCatalogObjectType type,
                           enum ngsOptionType optionType) const;

protected:
    GDALDataset* m_DS;
};

/**
 * @brief The Dataset class is base class of DataStore. Each table, raster,
 * feature class, etc. are Dataset. The DataStore is an array of Datasets as
 * Map is array of Layers.
 */
class Dataset : public ObjectContainer, public DatasetBase
{
    friend class FeatureClass;
    friend class Table;

public:
    explicit Dataset(ObjectContainer * const parent = nullptr,
            const enum ngsCatalogObjectType type = CAT_CONTAINER_ANY,
            const CPLString & name = "",
            const CPLString & path = "");
    virtual ~Dataset();
    virtual const char* options(enum ngsOptionType optionType) const override;

    TablePtr executeSQL(const char* statement, const char* dialect = "");
    TablePtr executeSQL(const char* statement,
                            GeometryPtr spatialFilter,
                            const char* dialect = "");


    virtual bool open(unsigned int openFlags,
                      const Options &options = Options()) override;
    virtual FeatureClass* createFeatureClass(const CPLString& name,
                                             enum ngsCatalogObjectType objectType,
                                             OGRFeatureDefn * const definition,
                                             OGRSpatialReference* spatialRef,
                                             OGRwkbGeometryType type,
                                             const Options& options = Options(),
                                             const Progress& progress = Progress());
    virtual Table* createTable(const CPLString& name,
                               enum ngsCatalogObjectType objectType,
                               OGRFeatureDefn * const definition,
                               const Options& options = Options(),
                               const Progress& progress = Progress());

    virtual bool setProperty(const char* key, const char* value);
    virtual CPLString property(const char* key, const char* defaultValue);
    virtual std::map<CPLString, CPLString> getProperties(
            const char* table, const char* domain);
    virtual void deleteProperties(const char* table);
    virtual void startBatchOperation() {}
    virtual void stopBatchOperation() {}
    virtual bool isBatchOperation() const { return false; }
    virtual void lockExecuteSql(bool lock);

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override { return access(m_path, W_OK) == 0; }
    virtual char** metadata(const char* domain) const override;

    // ObjectContainer interface
public:
    virtual bool hasChildren() override;
    virtual bool isReadOnly() const override;
    virtual int paste(ObjectPtr child, bool move = false,
                      const Options & options = Options(),
                      const Progress &progress = Progress()) override;
    virtual bool canPaste(const enum ngsCatalogObjectType type) const override;
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual void refresh() override;

    // static
public:
    static const char* additionsDatasetExtension();
    static const char* attachmentsFolderExtension();
    static Dataset* create(ObjectContainer * const parent,
                           const enum ngsCatalogObjectType type,
                           const CPLString & name,
                           const Options & options = Options());

protected:
    static OGRLayer* createMetadataTable(GDALDataset* ds);
    static bool destroyTable(GDALDataset* ds, OGRLayer* layer);
    static OGRLayer* createOverviewsTable(GDALDataset* ds, const char* name);
    static bool createOverviewsTableIndex(GDALDataset* ds, const char* name);
    static bool dropOverviewsTableIndex(GDALDataset* ds, const char* name);
    static OGRLayer* createAttachmentsTable(GDALDataset* ds, const char* path,
                                            const char* name);
    static OGRLayer* createEditHistoryTable(GDALDataset* ds, const char* name);

    //    static bool createEditHistoryTable(GDALDataset* ds, const char* name);

protected:
    virtual bool isNameValid(const char* name) const;
    virtual CPLString normalizeDatasetName(const CPLString& name) const;
    virtual CPLString normalizeFieldName(const CPLString& name) const;
    virtual void fillFeatureClasses();
    virtual bool destroyTable(Table* table);
    virtual GDALDataset* createAdditionsDataset();
    virtual OGRLayer* createOverviewsTable(const char* name);
    virtual bool destroyOverviewsTable(const char* name);
    virtual bool clearOverviewsTable(const char* name);
    virtual OGRLayer* getOverviewsTable(const char* name);
    virtual bool createOverviewsTableIndex(const char* name);
    virtual bool dropOverviewsTableIndex(const char* name);
    virtual OGRLayer* createAttachmentsTable(const char* name);
    virtual bool destroyAttachmentsTable(const char* name);
    virtual OGRLayer* getAttachmentsTable(const char* name);
    virtual const char* attachmentsTableName(const char* name) const;
    virtual OGRLayer* createEditHistoryTable(const char* name);
    virtual bool destroyEditHistoryTable(const char* name);
    virtual OGRLayer* getEditHistoryTable(const char* name);
    virtual void clearEditHistoryTable(const char* name);
    virtual const char* historyTableName(const char* name) const;
    virtual bool deleteFeatures(const char* name);

protected:
    GDALDataset* m_addsDS;
    OGRLayer* m_metadata;
    CPLMutex* m_executeSQLMutex;
};

/**
 * @brief The DatasetBatchOperationHolder class Holds batch operation start/stop
 */
class DatasetBatchOperationHolder
{
public:
    DatasetBatchOperationHolder(Dataset* dataset) : m_dataset(dataset) {
        if(nullptr != m_dataset)
            m_dataset->startBatchOperation();
    }

    ~DatasetBatchOperationHolder() {
        if(nullptr != m_dataset)
            m_dataset->stopBatchOperation();
    }

protected:
    Dataset* m_dataset;
};

/**
 * @brief The DatasetExecuteSQLLockHolder class lock sql excution in dataset
 */
class DatasetExecuteSQLLockHolder
{
public:
    DatasetExecuteSQLLockHolder(Dataset* dataset) : m_dataset(dataset) {
        if(nullptr != m_dataset)
            m_dataset->lockExecuteSql(true);
    }

    ~DatasetExecuteSQLLockHolder() {
        if(nullptr != m_dataset)
            m_dataset->lockExecuteSql(false);
    }

protected:
    Dataset* m_dataset;
};

}

#endif // NGSDATASET_H
