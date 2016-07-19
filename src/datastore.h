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
#ifndef DATASTORE_H
#define DATASTORE_H

#include "ogrsf_frmts.h"
#include "gdal_frmts.h"

#include <string>
#include <memory>

#include "dataset.h"
#include "api.h"

namespace ngs {

using namespace std;

typedef shared_ptr< OGRLayer > ResultSetPtr;

/**
 * @brief The main geodata storage and manipulation class
 */
class DataStore
{
    friend class Dataset;
    friend class Table;

public:
    enum ChangeType {
        ADD_FEATURE = ngsChangeCodes::CREATE_FEATURE,
        CHANGE_FEATURE = ngsChangeCodes::CHANGE_FEATURE,
        DELETE_FEATURE = ngsChangeCodes::DELETE_FEATURE,
        DELETEALL_FEATURES = ngsChangeCodes::DELETEALL_FEATURES,
        ADD_ATTACHMENT = ngsChangeCodes::CREATE_ATTACHMENT,
        CHANGE_ATTACHMENT = ngsChangeCodes::CHANGE_ATTACHMENT,
        DELETE_ATTACHMENT = ngsChangeCodes::DELETE_ATTACHMENT,
        DELETEALL_ATTACHMENTS = ngsChangeCodes::DELETEALL_ATTACHMENTS
    };

    enum StoreType {
        GPKG
    };

public:
    DataStore(const char* path);
    virtual ~DataStore();
    virtual int create(enum StoreType type);
    virtual int open();
    virtual int destroy();
    virtual int openOrCreate(enum StoreType type);
    virtual int datasetCount() const;
    virtual DatasetWPtr getDataset(const char* name);
    virtual DatasetWPtr getDataset(int index);
public:
    void setNotifyFunc(ngsNotifyFunc notifyFunc);
    void unsetNotifyFunc();
    virtual void onLowMemory() = 0;

protected:
    virtual int destroyDataset(enum Dataset::Type type, const CPLString& name);
    virtual void notifyDatasetChanged(enum ChangeType changeType,
                                      const CPLString& name, long id);
    virtual bool isNameValid(const CPLString &name) const;

    virtual ResultSetPtr executeSQL(const char* statement) const;
    static GDALDriver *getDriverForType(enum StoreType type);
protected:
    CPLString m_path;
    GDALDataset *m_DS;
    map<string, DatasetPtr> m_datasources;    
    ngsNotifyFunc m_notifyFunc;
};

typedef shared_ptr<DataStore> DataStorePtr;

/**
 * @brief The geodata storage and manipulation class for raster, vector geodata
 * and attachments
 */
class MobileDataStore : public DataStore
{
    friend class Dataset;
    friend class Table;

public:
    MobileDataStore(const char* path);
    virtual ~MobileDataStore();
    virtual int create(enum StoreType type) override;
    virtual int open() override;
    virtual int destroy() override;
    int createRemoteTMSRaster(const char* url, const char* name,
                              const char* alias, const char* copyright,
                              int epsg, int z_min, int z_max,
                              bool y_origin_top);
    virtual int datasetCount() const override;
    virtual DatasetWPtr getDataset(const char* name) override;
    virtual DatasetWPtr getDataset(int index) override;
    virtual ResultSetPtr executeSQL(const char *statement) const override;
    virtual void onLowMemory() override;

protected:
    int createMetadataTable();
    int createRastersTable();
    int createAttachmentsTable();
    int upgrade(int oldVersion);
    void setDataPath();

protected:
    virtual int destroyDataset(enum Dataset::Type type, const CPLString &name) override;
    virtual bool isNameValid(const CPLString &name) const override;

protected:
    CPLString m_dataPath;
};

}

#endif // DATASTORE_H
