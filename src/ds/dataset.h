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

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogrsf_frmts.h"

#include "api_priv.h"
#include "catalog/object.h"

namespace ngs {

typedef std::shared_ptr< OGRLayer > ResultSetPtr;

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

class IDataset {
public:
    virtual ~IDataset() = default;
    virtual bool isOpened() const = 0;
    virtual bool isReadOnly() const = 0;
    virtual bool open(unsigned int openFlags, char **options = nullptr) = 0;
};

//class Dataset;
//typedef std::shared_ptr<Dataset> DatasetPtr;
// typedef weak_ptr<Dataset> DatasetWPtr;

/**
 * @brief The Dataset class is base class of DataStore. Each table, raster,
 * feature class, etc. are Dataset. The DataStore is an array of Datasets as
 * Map is array of Layers.
 */
class Dataset : public Object, public IDataset
{
    friend class DatasetContainer;
    friend class DataStore;

public:
    Dataset(ObjectContainer * const parent = nullptr,
            const ngsCatalogObjectType type = CAT_FC_ANY,
            const CPLString & name = "",
            const CPLString & path = "");
    virtual ~Dataset();

    // is checks
    virtual bool isOpened() const override { return m_DS != nullptr; }
    virtual bool isReadOnly() const override { return false; }
    virtual bool open(unsigned int openFlags, char **options = nullptr) override;

    // Object interface
public:
    virtual bool destroy() override;

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

protected:
    GDALDataset* m_DS;
};

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

}

#endif // NGSDATASET_H
