/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
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
#ifndef NGSDATASTORE_H
#define NGSDATASTORE_H

#include "store.h"

namespace ngs {

constexpr const char *TRACKS_POINTS_TABLE = "nga_tracks_pt";
constexpr const char *TRACKS_TABLE = "nga_tracks";

/**
 * @brief The storage and manipulation class for raster and vector spatial data
 * and attachments
 */
class DataStore : public Dataset, public SpatialDataset,
        public StoreObjectContainer
{
    friend class FeatureClassOverview;
public:
    explicit DataStore(ObjectContainer * const parent = nullptr,
              const std::string &name = "",
              const std::string &path = "");
    virtual ~DataStore() override = default;
    bool hasTracksTable() const;
    ObjectPtr getTracksTable();
    bool destroyTracksTable();

    // static
public:
    static bool create(const std::string &path);
    static std::string extension();

    // Dataset interface
public:
    virtual bool open(unsigned int openFlags = DatasetBase::defaultOpenFlags,
                      const Options &options = Options()) override;
    virtual void startBatchOperation() override { enableJournal(false); }
    virtual void stopBatchOperation() override { enableJournal(true); }
    virtual bool isBatchOperation() const override;

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

    // Object interface
public:
    virtual bool setProperty(const std::string &key, const std::string &value,
                             const std::string &domain = NG_ADDITIONS_KEY) override;

    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type,
                        const std::string& name,
                        const Options &options) override;

    // static
protected:
    static OGRLayer *createOverviewsTable(GDALDataset *ds,
                                          const std::string &name);
    static bool createOverviewsTableIndex(GDALDataset *ds,
                                          const std::string &name);
    static bool dropOverviewsTableIndex(GDALDataset *ds,
                                        const std::string &name);
    // StoreObjectContainer interface
public:
    virtual bool sync() override;

protected:
    virtual bool isNameValid(const std::string &name) const override;
    virtual std::string normalizeFieldName(const std::string &name,
                                           const std::vector<std::string> &nameList,
                                           int counter = 0) const override;
    virtual void fillFeatureClasses() const override;
    bool createTracksTable();

    virtual OGRLayer *createOverviewsTable(const std::string &name);
    virtual bool destroyOverviewsTable(const std::string &name);
    virtual bool clearOverviewsTable(const std::string &name);
    virtual OGRLayer *getOverviewsTable(const std::string &name);
    virtual bool createOverviewsTableIndex(const std::string &name);
    virtual bool dropOverviewsTableIndex(const std::string &name);
    virtual std::string overviewsTableName(const std::string &name) const;

protected:
    void enableJournal(bool enable);
    bool upgrade(int oldVersion);

protected:
    unsigned char m_disableJournalCounter;
    ObjectPtr m_tracksTable;

};

} // namespace ngs

#endif // NGSDATASTORE_H
