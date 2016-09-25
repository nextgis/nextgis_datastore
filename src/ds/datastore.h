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
    virtual int destroy(ProgressInfo* progressInfo = nullptr) override;
    virtual int datasetCount() const override;
    virtual int rasterCount() const override;
    virtual DatasetPtr getDataset(const CPLString& name) override;
    virtual DatasetPtr getDataset(int index) override;
    virtual DatasetPtr createDataset(const CPLString &name,
                                     OGRFeatureDefn* const definition,
                                     const OGRSpatialReference *,
                                     OGRwkbGeometryType type = wkbUnknown,
                                     ProgressInfo *progressInfo = nullptr) override;
    virtual int copyDataset(DatasetPtr srcDataset, const CPLString& dstName,
                            LoadData *loadData = nullptr) override;

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

    // DatasetContainer interface
public:
    virtual const char *getOptions(ngsDataStoreOptionsTypes optionType) const override;

protected:
    // TODO: create changes layer CPLString changesLayerName = name + "_changes";
    static int createMetadataTable(GDALDatasetPtr ds);
    static int createRastersTable(GDALDatasetPtr ds);
    static int createAttachmentsTable(GDALDatasetPtr ds);
    static int upgrade(int oldVersion, GDALDatasetPtr ds);
    void setDataPath();
    virtual bool isNameValid(const CPLString &name) const override;
    void enableJournal(bool enable);

protected:
    CPLString m_dataPath;
    unsigned char m_disableJournalCounter;
    OGRSpatialReference m_storeSpatialRef;

};

}

#endif // DATASTORE_H
