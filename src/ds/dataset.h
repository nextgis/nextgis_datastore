/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
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

#include <util/mutex.h>

#include "api_priv.h"
#include "featureclass.h"
#include "geometry.h"

#include "catalog/objectcontainer.h"
#include "ngstore/codes.h"
#include "util/stringutil.h"

namespace ngs {

constexpr const char *OVR_ZOOM_KEY = "z";
constexpr const char *OVR_X_KEY = "x";
constexpr const char *OVR_Y_KEY = "y";
constexpr const char *OVR_TILE_KEY = "tile";
constexpr const char *ATTACH_FEATURE_ID_FIELD = "afid";
constexpr const char *ATTACH_FILE_NAME_FIELD = "name";
constexpr const char *ATTACH_DESCRIPTION_FIELD = "descript";
constexpr const char *FEATURE_ID_FIELD = "ffid";
constexpr const char *OPERATION_FIELD = "op";

constexpr const char *USER_KEY = "user";
constexpr const char *NG_ADDITIONS_KEY = "nga";
constexpr const char *USER_PREFIX_KEY = "USER.";
constexpr int USER_PREFIX_KEY_LEN = length(USER_PREFIX_KEY);
constexpr const char *NGS_VERSION_KEY = "version";


/**
 * @brief The wrapper class around GDALDataset pointer
 */
class GDALDatasetPtr : public std::shared_ptr<GDALDataset>
{
public:
    explicit GDALDatasetPtr(GDALDataset *ds);
    GDALDatasetPtr();
    explicit GDALDatasetPtr(const GDALDatasetPtr &ds);
    GDALDatasetPtr &operator=(GDALDataset *ds);
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
    virtual std::string options(enum ngsOptionType optionType) const = 0;
    virtual GDALDataset *getGDALDataset() const { return m_DS; }
    virtual bool open(unsigned int openFlags = GDAL_OF_SHARED|GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR,
                      const Options &options = Options()) = 0;
    virtual void close();
    bool startTransaction(bool force = false);
    bool commitTransaction();
    bool rollbackTransaction();
    void flushCache();
    // is checks
    virtual bool isOpened() const { return m_DS != nullptr; }
protected:
    bool open(const std::string &path, unsigned int openFlags,
              const Options &options = Options());
    std::string options(const enum ngsCatalogObjectType type,
                           enum ngsOptionType optionType) const;

protected:
    GDALDataset *m_DS;
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
            const std::string &name = "",
            const std::string &path = "");
    virtual ~Dataset() override;
    virtual std::string options(enum ngsOptionType optionType) const override;

    TablePtr executeSQL(const std::string &statement,
                        const std::string &dialect = "");
    TablePtr executeSQL(const std::string &statement, GeometryPtr spatialFilter,
                        const std::string &dialect = "");


    virtual bool open(unsigned int openFlags = GDAL_OF_SHARED|GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR,
                      const Options &options = Options()) override;
    virtual FeatureClass *createFeatureClass(const std::string &name,
                                             enum ngsCatalogObjectType objectType,
                                             OGRFeatureDefn * const definition,
                                             SpatialReferencePtr spatialRef,
                                             OGRwkbGeometryType type,
                                             const Options &options = Options(),
                                             const Progress &progress = Progress());
    virtual Table *createTable(const std::string &name,
                               enum ngsCatalogObjectType objectType,
                               OGRFeatureDefn * const definition,
                               const Options &options = Options(),
                               const Progress &progress = Progress());

    virtual void startBatchOperation() {}
    virtual void stopBatchOperation() {}
    virtual bool isBatchOperation() const { return false; }
    virtual void lockExecuteSql(bool lock);

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool setProperty(const std::string &key, const std::string &value,
                             const std::string &domain) override;
    virtual void deleteProperties(const std::string &domain) override;

    // ObjectContainer interface
public:
    virtual bool isReadOnly() const override;
    virtual int paste(ObjectPtr child, bool move = false,
                      const Options &options = Options(),
                      const Progress &progress = Progress()) override;
    virtual bool canPaste(const enum ngsCatalogObjectType type) const override;
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual void refresh() override;
    virtual bool loadChildren() override;

    // static
public:
    static std::string additionsDatasetExtension();
    static std::string attachmentsFolderExtension();
    static Dataset *create(ObjectContainer * const parent,
                           const enum ngsCatalogObjectType type,
                           const std::string &name,
                           const Options &options = Options());

protected:
    static OGRLayer *createMetadataTable(GDALDataset *ds);
    static bool destroyTable(GDALDataset *ds, OGRLayer *layer);
    static OGRLayer *createOverviewsTable(GDALDataset *ds,
                                          const std::string &name);
    static bool createOverviewsTableIndex(GDALDataset *ds,
                                          const std::string &name);
    static bool dropOverviewsTableIndex(GDALDataset *ds,
                                        const std::string &name);
    static OGRLayer *createAttachmentsTable(GDALDataset *ds,
                                            const std::string &path,
                                            const std::string &name);
    static OGRLayer *createEditHistoryTable(GDALDataset *ds,
                                            const std::string &name);

    //    static bool createEditHistoryTable(GDALDataset* ds, const char* name);

protected:
    virtual bool isNameValid(const std::string &name) const;
    virtual std::string normalizeDatasetName(const std::string &name) const;
    virtual std::string normalizeFieldName(const std::string &name) const;
    virtual void fillFeatureClasses() const;
    virtual bool skipFillFeatureClass(OGRLayer *layer) const;
    virtual bool destroyTable(Table *table);
    virtual GDALDataset *createAdditionsDataset();
    virtual OGRLayer *createOverviewsTable(const std::string &name);
    virtual bool destroyOverviewsTable(const std::string &name);
    virtual bool clearOverviewsTable(const std::string &name);
    virtual OGRLayer *getOverviewsTable(const std::string &name);
    virtual bool createOverviewsTableIndex(const std::string &name);
    virtual bool dropOverviewsTableIndex(const std::string &name);
    virtual std::string overviewsTableName(const std::string &name) const;
    virtual OGRLayer *createAttachmentsTable(const std::string &name);
    virtual bool destroyAttachmentsTable(const std::string &name);
    virtual OGRLayer *getAttachmentsTable(const std::string &name);
    virtual std::string attachmentsTableName(const std::string &name) const;
    virtual OGRLayer *createEditHistoryTable(const std::string &name);
    virtual bool destroyEditHistoryTable(const std::string &name);
    virtual OGRLayer *getEditHistoryTable(const std::string &name);
    virtual void clearEditHistoryTable(const std::string &name);
    virtual std::string historyTableName(const std::string &name) const;
    virtual bool deleteFeatures(const std::string &name);

protected:
    GDALDataset *m_addsDS;
    OGRLayer *m_metadata;
    Mutex m_executeSQLMutex;
};

/**
 * @brief The DatasetBatchOperationHolder class Holds batch operation start/stop
 */
class DatasetBatchOperationHolder
{
public:
    DatasetBatchOperationHolder(Dataset *dataset);
    ~DatasetBatchOperationHolder();

protected:
    Dataset *m_dataset;
};

/**
 * @brief The DatasetExecuteSQLLockHolder class lock sql excution in dataset
 */
class DatasetExecuteSQLLockHolder
{
public:
    DatasetExecuteSQLLockHolder(Dataset* dataset);
    ~DatasetExecuteSQLLockHolder();

protected:
    Dataset *m_dataset;
};

}

#endif // NGSDATASET_H
