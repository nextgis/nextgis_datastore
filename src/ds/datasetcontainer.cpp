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
#include "datasetcontainer.h"
#include "featuredataset.h"

using namespace ngs;

void ngs::LoadingThread(void * store)
{
    /*DataStore* pDataStore = static_cast<DataStore*>(store);
    double totalDataCount;
    double loadedDataCount = 0;
    while(!pDataStore->m_cancelLoad || pDataStore->m_loadData.size () > 0) {
        totalDataCount = pDataStore->m_loadData.size () + loadedDataCount;
        double percent = loadedDataCount / totalDataCount;
        LoadData data = pDataStore->m_loadData[pDataStore->m_loadData.size () - 1];
        pDataStore->m_loadData.pop_back ();
/*
        if(data.progressFunc)
            if(data.progressFunc(percent, CPLSPrintf ("Start loading '%s'",
                    CPLGetFilename(data.path)), data.progressArguments) == FALSE)
                continue;

        // 1. Open
        GDALDataset* pDS = static_cast<GDALDataset*>( GDALOpenEx(data.path,
                        GDAL_OF_SHARED|GDAL_OF_READONLY, nullptr, nullptr, nullptr) );
        if(nullptr == pDS) {
            if(data.progressFunc)
                data.progressFunc(percent, CPLSPrintf ("Dataset '%s' open failed",
                            CPLGetFilename(data.path)), data.progressArguments);
                    continue;
        }
        // 2. Analyse structure, etc,

        // 3. Check name and create layer in store
        // 4. for each feature
        size_t featureCount;
        double featureRead = 0;
        double subPercent = featureRead / featureCount;
        percent = percent + subPercent / totalDataCount;
        // 4.1. read
        // 4.2. create samples for several scales
        // 4.3. create feature in storage dataset
        // 4.4. create mapping of fields and original spatial reference metadata

    }*/
}

//------------------------------------------------------------------------------
// DatasetContainer
//------------------------------------------------------------------------------


DatasetContainer::DatasetContainer() : Dataset(), m_hLoadThread(nullptr),
    m_cancelLoad(false)
{

}

DatasetContainer::~DatasetContainer()
{
    if(nullptr != m_hLoadThread) {
        m_cancelLoad = true;
        // wait thread exit
        CPLJoinThread(m_hLoadThread);
    }
}

int DatasetContainer::datasetCount() const
{
    return m_DS->GetLayerCount ();
}

int DatasetContainer::rasterCount() const
{
    char** subdatasetList = m_DS->GetMetadata ("SUBDATASETS");
    int count = CSLCount(subdatasetList);
    int out = 0;
    size_t strLen = 0;
    const char* testStr = nullptr;
    for(int i = 0; i < count; ++i){
        testStr = subdatasetList[i];
        strLen = CPLStrnlen(testStr, 255);
        if(EQUAL(testStr + strLen - 4, "NAME"))
            out++;
    }
    CSLDestroy(subdatasetList);

    return out;
}

DatasetWPtr DatasetContainer::getDataset(const CPLString &name)
{
    DatasetPtr dataset;
    auto it = m_datasets.find (name);
    if( it != m_datasets.end ()){
        if(!it->second->isDeleted ()){
            return it->second;
        }
        else{
            return dataset;
        }
    }

    OGRLayer* pLayer = m_DS->GetLayerByName (name);
    if(nullptr != pLayer){
        Dataset* pDataset = nullptr;
        if( EQUAL(pLayer->GetGeometryColumn (), "")){
            pDataset = new Table(pLayer);
        }
        else {
            pDataset = new FeatureDataset(pLayer);
        }

        pDataset->setNotifyFunc (m_notifyFunc);
        dataset.reset(pDataset);
        if(pDataset)
            m_datasets[dataset->name ()] = dataset;
    }

    return dataset;
}

// TODO: getRaster(int index)
// TODO: getRaster(const CPLString &name)
DatasetWPtr DatasetContainer::getDataset(int index)
{
    for(int i = 0; i < m_DS->GetLayerCount (); ++i){
        OGRLayer* pLayer = m_DS->GetLayer (i);
        if( i == index)
            return getDataset (pLayer->GetName());
    }
    return DatasetWPtr();
}

bool DatasetContainer::isNameValid(const CPLString &name) const
{
    return m_DS->GetLayerByName (name) == nullptr;
}

int DatasetContainer::loadDataset(const CPLString& path, const
                                  CPLString& subDatasetName,
                                  ngsProgressFunc progressFunc,
                                  void* progressArguments)
{
    // TODO: DataStore is virtual
    /*m_loadData.push_back ({make_shared<DataStore>(path), subDatasetName,
                           progressFunc, progressArguments});
    if(nullptr == m_hLoadThread) {
        m_hLoadThread = CPLCreateJoinableThread(LoadingThread, this);
    }*/

    return ngsErrorCodes::SUCCESS;
}
