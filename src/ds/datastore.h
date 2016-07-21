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

#include "datasetcontainer.h"

namespace ngs {

using namespace std;

class DataStore;
typedef shared_ptr<DataStore> DataStorePtr;

/**
 * @brief The geodata storage and manipulation class for raster, vector geodata
 * and attachments
 */
class DataStore : public DatasetContainer
{
public:
    DataStore();
    virtual ~DataStore();

    // overrides
    virtual int destroy(ngsProgressFunc progressFunc = nullptr,
                        void* progressArguments = nullptr) override;
    virtual int datasetCount() const override;
    virtual int rasterCount() const override;
    virtual DatasetWPtr getDataset(int index) override;

    // static
    static DataStorePtr openOrCreate(const CPLString& path);
    static DataStorePtr create(const CPLString& path);
    static DataStorePtr open(const CPLString& path);

    //
    int createRemoteTMSRaster(const char* url, const char* name,
                              const char* alias, const char* copyright,
                              int epsg, int z_min, int z_max,
                              bool y_origin_top);

    ResultSetPtr executeSQL(const CPLString& statement) const;
    void onLowMemory();

protected:
    static int createMetadataTable(GDALDatasetPtr ds);
    static int createRastersTable(GDALDatasetPtr ds);
    static int createAttachmentsTable(GDALDatasetPtr ds);
    static int upgrade(int oldVersion, GDALDatasetPtr ds);
    void setDataPath();
    virtual bool isNameValid(const CPLString &name) const override;

protected:
    CPLString m_dataPath;
};

/* TODO: createDataset() with history also add StoreFeatureDataset to override copyRows function
// 3. Analyse structure, etc,
// 4. for each feature
// 4.1. read
// 4.2. create samples for several scales
// 4.3. create feature in storage dataset
// 4.4. create mapping of fields and original spatial reference metadata
*/

}

#endif // DATASTORE_H
