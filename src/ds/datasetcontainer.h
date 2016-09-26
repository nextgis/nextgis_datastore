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
#ifndef DATASETCONTAINER_H
#define DATASETCONTAINER_H

#include "dataset.h"

namespace ngs {

void LoadingThread(void * store);

class LoadData : public ProgressInfo
{
public:
    LoadData(unsigned int id, const CPLString& path,
             const CPLString& srcSubDatasetName,
             const CPLString& dstDatasetName,
             const char** options = nullptr, ngsProgressFunc onProgress = nullptr,
             void *progressArguments = nullptr);
    ~LoadData();
    LoadData(const LoadData& data);
    LoadData& operator=(const LoadData& data);
    CPLString dstDatasetName() const;
    CPLString path() const;
    CPLString srcSubDatasetName() const;
    void addNewName(const CPLString& name);
    const char* getNewNames() const;
private:
    CPLString m_path;
    CPLString m_srcSubDatasetName;
    CPLString m_dstDatasetName;
    vector<CPLString> m_dstDatasetNewNames;
    CPLString m_newNames;
};

class DatasetContainer : public Dataset
{
    friend void LoadingThread(void * store);
public:
    DatasetContainer();
    virtual ~DatasetContainer();
    virtual int datasetCount() const;
    virtual int rasterCount() const;
    virtual DatasetPtr getDataset(const CPLString& name);
    virtual DatasetPtr getDataset(int index);
    // TODO: getRaster
    unsigned int loadDataset(const CPLString& name, const CPLString& path,
                             const CPLString& subDatasetName, const char **options,
                             ngsProgressFunc progressFunc,
                             void* progressArguments = nullptr);
    /* TODO: does this need here?
    bool canCopy(const CPLString &destPath);
    bool canMove(const CPLString &destPath);
    */
    /* TODO: release this
    int move(const CPLString &destPath, ngsProgressFunc progressFunc = nullptr,
             void* progressArguments = nullptr);
    int copy(const CPLString &destPath, ngsProgressFunc progressFunc = nullptr,
             void* progressArguments = nullptr);
    */
    // TODO: create layer options, copy options
    virtual int copyDataset(DatasetPtr srcDataset, const CPLString &dstDatasetName,
                            LoadData *loadData = nullptr);
    virtual int moveDataset(DatasetPtr srcDataset, const CPLString& dstDatasetName,
                            LoadData *loadData = nullptr);
    virtual DatasetPtr createDataset(const CPLString &name,
                                     OGRFeatureDefn* const definition,
                                     ProgressInfo *progressInfo = nullptr);
    virtual DatasetPtr createDataset(const CPLString &name,
                                     OGRFeatureDefn* const definition,
                                     const OGRSpatialReference *spatialRef,
                                     OGRwkbGeometryType type,
                                     ProgressInfo *progressInfo = nullptr);
    ngsLoadTaskInfo getLoadTaskInfo (unsigned int taskId) const;
    virtual const char* getOptions(ngsDataStoreOptionsTypes optionType) const;
protected:
    virtual bool isNameValid(const CPLString& name) const;
    virtual CPLString normalizeDatasetName(const CPLString& name) const;
    virtual CPLString normalizeFieldName(const CPLString& name) const;
    bool isDatabase() const;
    vector<OGRwkbGeometryType> getGeometryTypes(DatasetPtr srcDataset);
protected:
    map<string, DatasetPtr> m_datasets;
    /**
     * Load Dataset
     */
    CPLJoinableThread* m_hLoadThread;
    bool m_cancelLoad;
    vector<LoadData> m_loadData;
    unsigned int m_newTaskId;

    // Dataset interface
public:
    virtual int destroy(ProgressInfo *processInfo) override;
};

typedef shared_ptr<DatasetContainer> DatasetContainerPtr;
}

#endif // DATASETCONTAINER_H
