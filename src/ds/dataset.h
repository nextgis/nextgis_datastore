/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
#ifndef NGSDATASET_H
#define NGSDATASET_H

#include <memory>

#include "ogrsf_frmts.h"

#include "api_priv.h"
#include "geometry.h"
#include "featureclass.h"
#include "raster.h"
#include "table.h"
#include "catalog/objectcontainer.h"

namespace ngs {

/**
 * @brief The wrapper class around GDALDataset pointer
 */
class GDALDatasetPtr : public std::shared_ptr< GDALDataset >
{
public:
    GDALDatasetPtr(GDALDataset* ds);
    GDALDatasetPtr();
    GDALDatasetPtr(const GDALDatasetPtr& ds);
    GDALDatasetPtr& operator=(GDALDataset* ds);
    operator GDALDataset*() const;
};

/**
 * @brief The Dataset class is base class of DataStore. Each table, raster,
 * feature class, etc. are Dataset. The DataStore is an array of Datasets as
 * Map is array of Layers.
 */
class Dataset : public ObjectContainer
{
public:
    Dataset(ObjectContainer * const parent = nullptr,
            const ngsCatalogObjectType type = ngsCatalogObjectType::CAT_CONTAINER_ANY,
            const CPLString & name = "",
            const CPLString & path = "");
    virtual ~Dataset();
    virtual const char* getOptions(enum ngsOptionTypes optionType) const;
    virtual GDALDataset * getGDALDataset() const { return m_DS; }

    TablePtr executeSQL(const char* statement, const char* dialect = "");
    TablePtr executeSQL(const char* statement,
                            GeometryPtr spatialFilter,
                            const char* dialect = "");

    // is checks
    virtual bool isOpened() const { return m_DS != nullptr; }
    virtual bool isReadOnly() const { return m_readonly; }
    virtual bool open(unsigned int openFlags, const Options &options = Options());

    // Object interface
public:
    virtual bool destroy() override;

    // ObjectContainer interface
public:
    virtual bool hasChildren() override;

protected:
    virtual bool isNameValid(const char *name) const;
    virtual CPLString normalizeDatasetName(const CPLString& name) const;
    virtual CPLString normalizeFieldName(const CPLString& name) const;

protected:
    GDALDataset* m_DS;
    bool m_readonly;
};

}

#endif // NGSDATASET_H

//protected:
//    static DatasetPtr getDatasetForGDAL(const CPLString& path, GDALDatasetPtr ds);
//    inline int reportError(int code, double percent,
//                           const char* message, ProgressInfo *processInfo = nullptr) {
//        //CPLError(CE_Failure, CPLE_AppDefined, message);
//        CPLErrorSetState(CE_Failure, CPLE_AppDefined, message);
//        if(processInfo) {
//            processInfo->setStatus (static_cast<ngsErrorCodes>(code));
//            processInfo->onProgress (percent, message);
//        }
//        return code;
//    }
//class ProgressInfo
//{
//public:
//    ProgressInfo(unsigned int id, const char **options = nullptr,
//                 ngsProgressFunc progressFunc = nullptr,
//                 void *progressArguments = nullptr);
//    virtual ~ProgressInfo();
//    ProgressInfo(const ProgressInfo& data);
//    ProgressInfo& operator=(const ProgressInfo& data);
//    unsigned int id() const;
//    char **options() const;
//    bool onProgress(double complete, const char* message) const;
//    ngsErrorCodes status() const;
//    void setStatus(const ngsErrorCodes &status);
//protected:
//    unsigned int m_id;
//    char** m_options;
//    ngsProgressFunc m_progressFunc;
//    void *m_progressArguments;
//    enum ngsErrorCodes m_status;
//};

//void LoadingThread(void * store);

//class LoadData : public ProgressInfo
//{
//public:
//    LoadData(unsigned int id, const CPLString& path,
//             const CPLString& srcSubDatasetName,
//             const CPLString& dstDatasetName,
//             const char** options = nullptr, ngsProgressFunc onProgress = nullptr,
//             void *progressArguments = nullptr);
//    ~LoadData();
//    LoadData(const LoadData& data);
//    LoadData& operator=(const LoadData& data);
//    CPLString srcSubDatasetName() const;
//    void addNewName(const CPLString& name);
//    const char* path() const;
//    const char* getNewNames() const;
//    const char* getDestinationName() const;
//private:
//    CPLString m_path;
//    CPLString m_srcSubDatasetName;
//    CPLString m_dstDatasetName;
//    CPLString m_newNames;
//};

//    unsigned int loadDataset(const CPLString& name, const CPLString& path,
//                             const CPLString& subDatasetName, const char **options,
//                             ngsProgressFunc progressFunc,
//                             void* progressArguments = nullptr);
    // TODO: create layer options, copy options
//    virtual int copyDataset(DatasetPtr srcDataset, const CPLString &dstDatasetName,
//                            LoadData *loadData = nullptr);
//    virtual int moveDataset(DatasetPtr srcDataset, const CPLString& dstDatasetName,
//                            LoadData *loadData = nullptr);
//    virtual DatasetPtr createDataset(const CPLString &name,
//                                     OGRFeatureDefn* const definition,
//                                     ProgressInfo *progressInfo = nullptr);
//    virtual DatasetPtr createDataset(const CPLString &name,
//                                     OGRFeatureDefn* const definition,
//                                     const OGRSpatialReference *spatialRef,
//                                     OGRwkbGeometryType type,
//                                     ProgressInfo *progressInfo = nullptr);
//    ngsLoadTaskInfo getLoadTaskInfo (unsigned int taskId) const;

/**
 * Load Dataset
 */
//CPLJoinableThread* m_hLoadThread;
//bool m_cancelLoad;
//std::vector<LoadData> m_loadData;
//unsigned int m_newTaskId;
