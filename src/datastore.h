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

class MapStore;

/**
 * @brief The main geodata storage and manipulation class
 */
class DataStore
{
    friend class Dataset;
    friend class Table;
    friend class MapStore;

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

public:
    DataStore(const char* path, const char* dataPath, const char* cachePath);
    ~DataStore();
    int create();
    int open();
    int destroy();
    int openOrCreate();
    int createRemoteTMSRaster(const char* url, const char* name,
                              const char* alias, const char* copyright,
                              int epsg, int z_min, int z_max,
                              bool y_origin_top);
    int datasetCount() const;
    DatasetWPtr getDataset(const char* name);
    DatasetWPtr getDataset(int index);
public:
    string formats();
    void onLowMemory();
    void setNotifyFunc(const ngsNotifyFunc &notifyFunc);
    void unsetNotifyFunc();

protected:
    void initGDAL();
    void registerDrivers();
    int createMetadataTable();
    int createRastersTable();
    int createAttachmentsTable();
    int createMapsTable();
    int createLayersTable();
    int upgrade(int oldVersion);
    int destroyDataset(enum Dataset::Type type, const string& name);
    void notifyDatasetCanged(enum ChangeType changeType, const string& name,
                             long id);
    bool isNameValid(const string& name) const;

    ResultSetPtr executeSQL(const string& statement) const;
protected:
    string m_path;
    string m_cachePath;
    string m_dataPath;
    GDALDataset *m_DS;
    bool m_driversLoaded;
    map<string, DatasetPtr> m_datasources;    
    ngsNotifyFunc m_notifyFunc;
};

typedef shared_ptr<DataStore> DataStorePtr;

}

#endif // DATASTORE_H
