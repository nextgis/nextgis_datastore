/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019 NextGIS, <info@nextgis.com>
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
#ifndef MAPINFODATASTORE_H
#define MAPINFODATASTORE_H

#include "storefeatureclass.h"

namespace ngs {

/**
 * The MapInfoDataStore class
 */
class MapInfoDataStore : public Dataset, public SpatialDataset,
        public StoreObjectContainer
{
    friend class MapInfoStoreFeatureClass;
    friend class MapInfoStoreTable;
public:
    explicit MapInfoDataStore(ObjectContainer * const parent = nullptr,
              const std::string &name = "",
              const std::string &path = "");
    // static
public:
    static bool create(const std::string &path);
    static std::string extension();

    // Dataset interface
public:
    virtual bool open(unsigned int openFlags = DatasetBase::defaultOpenFlags,
                      const Options &options = Options()) override;
    virtual FeatureClass *createFeatureClass(const std::string &name,
                                             enum ngsCatalogObjectType objectType,
                                             OGRFeatureDefn * const definition,
                                             SpatialReferencePtr spatialRef,
                                             OGRwkbGeometryType type,
                                             const Options &options = Options(),
                                             const Progress &progress = Progress()) override;
    virtual Table *createTable(const std::string& name,
                               enum ngsCatalogObjectType objectType,
                               OGRFeatureDefn * const definition,
                               const Options &options = Options(),
                               const Progress &progress = Progress()) override;
    // Dataset interface
protected:
    virtual OGRLayer *createAttachmentsTable(const std::string &name) override;
    virtual OGRLayer *createEditHistoryTable(const std::string &name) override;
    virtual std::string normalizeFieldName(const std::string &name,
                                           const std::vector<std::string> &nameList,
                                           int counter = 0) const override;
    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type,
                             const std::string& name,
                             const Options &options) override;

    // StoreObjectContainer interface
public:
    virtual bool sync() override;

    // Dataset interface
protected:
    virtual std::string additionsDatasetPath() const override;
    virtual std::string attachmentsFolderPath(bool create) const override;

protected:
    virtual void fillFeatureClasses() const override;

protected:
    bool upgrade(int oldVersion);
    OGRLayer *getHashTable(const std::string &name);
    OGRLayer *createHashTable(const std::string &name);
    void clearHashTable(const std::string &name);
    bool destroyHashTable(const std::string &name);
    std::string tempPath() const;
};

/**
 * MapInfoStoreTable class
 */
class MapInfoStoreTable : public Table, public StoreObject
{
public:
    MapInfoStoreTable(GDALDatasetPtr DS, OGRLayer *layer,
                      ObjectContainer * const parent = nullptr,
                      const std::string &path = "",
                      const std::string &encoding = "CP1251");
    virtual ~MapInfoStoreTable() override;

    //Object interface
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool destroy() override;

    // Table interface
protected:
    virtual std::string storeName() const override;
    virtual bool checkSetProperty(const std::string &key,
                                  const std::string &value,
                                  const std::string &domain) override;

    // StoreObject
public:
    virtual bool sync() override;
    virtual FeaturePtr getFeatureByRemoteId(GIntBig rid) const override;

protected:
    void close();

private:
    GDALDatasetPtr m_TABDS;
    std::string m_storeName;
    std::string m_encoding;
};

/**
 * MapInfoStoreFeatureClass class
 */
class MapInfoStoreFeatureClass : public FeatureClass, public StoreObject
{
public:
    MapInfoStoreFeatureClass(GDALDatasetPtr DS, OGRLayer *layer,
                             ObjectContainer * const parent = nullptr,
                             const std::string &path = "",
                             const std::string &encoding = "CP1251");

    //Object interface
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool destroy() override;

    // Table interface
public:
    virtual bool insertFeature(const FeaturePtr &feature, bool logEdits) override;
    virtual bool updateFeature(const FeaturePtr &feature, bool logEdits) override;
    virtual bool deleteFeature(GIntBig id, bool logEdits) override;
    virtual bool deleteFeatures(bool logEdits) override;
    virtual std::vector<ngsEditOperation> editOperations() override;
    // Table interface
protected:
    virtual std::string storeName() const override;
    virtual bool checkSetProperty(const std::string &key,
                                  const std::string &value,
                                  const std::string &domain) override;

    // Table interface
 public:
    virtual void onRowCopied(FeaturePtr srcFeature, FeaturePtr dstFature,
                             const Options &options = Options()) override;
    virtual bool onRowsCopied(const TablePtr srcTable, const Progress &progress,
                              const Options &options) override;

   // StoreObject interface
public:
    virtual bool sync() override;
    virtual FeaturePtr getFeatureByRemoteId(GIntBig rid) const override;

protected:
    void close();
    int fillHash(const Progress &progress, const Options &options);
    bool updateHashAndEditLog();

private:
   GDALDatasetPtr m_TABDS;
   std::string m_storeName;
   std::string m_encoding;
};

} // namespace ngs

#endif // MAPINFODATASTORE_H
