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
    DatasetContainer* pDataset = static_cast<DatasetContainer*>(store);
    while(!pDataset->m_cancelLoad && !pDataset->m_loadData.empty ()) {
        LoadData data = pDataset->m_loadData[pDataset->m_loadData.size () - 1];
        pDataset->m_loadData.pop_back ();

        if(data.progressFunc)
            if(data.progressFunc(0, CPLSPrintf ("Start loading '%s'",
                    CPLGetFilename(data.path)), data.progressArguments) == FALSE)
                continue;



        // 1. Open
        DatasetPtr DS = Dataset::open (data.path, GDAL_OF_SHARED|GDAL_OF_READONLY,
                                       nullptr);
        if(nullptr == DS) {
            if(data.progressFunc)
                data.progressFunc(0, CPLSPrintf ("Dataset '%s' open failed",
                            CPLGetFilename(data.path)), data.progressArguments);
                    continue;
        }

        if(DS->type () == Dataset::CONTAINER) {
            DatasetContainer* pDatasetContainer = static_cast<
                    DatasetContainer*>(DS.get ());
            DS = pDatasetContainer->getDataset (data.subDatasetName).lock ();
        }

        // 2. Move or cope ds to container
        if(data.move && DS->canDelete ()) {
            pDataset->moveDataset(DS, data.progressFunc, data.progressArguments);
        }
        else {
            pDataset->copyDataset(DS, data.progressFunc, data.progressArguments);
        }
    }
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

    OGRLayer* layer = m_DS->GetLayerByName (name);
    if(nullptr != layer){
        Dataset* pDataset = nullptr;
        if( EQUAL(layer->GetGeometryColumn (), "")){
            pDataset = new Table(layer);
        }
        else {
            pDataset = new FeatureDataset(layer);
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
                                  CPLString& subDatasetName, bool move,
                                  ngsProgressFunc progressFunc,
                                  void* progressArguments)
{
    m_loadData.push_back ({path, subDatasetName, move,
                           progressFunc, progressArguments});
    if(nullptr == m_hLoadThread) {
        m_hLoadThread = CPLCreateJoinableThread(LoadingThread, this);
    }
    return ngsErrorCodes::SUCCESS;
}

int DatasetContainer::copyDataset(DatasetPtr srcDs, ngsProgressFunc progressFunc,
                                  void *progressArguments)
{
    // create uniq name
    CPLString name = srcDs->name ();
    if(name.empty ())
        name = "new_dataset";
    CPLString newName = name;
    int counter = 0;
    while(!isNameValid (newName)) {
        newName = name + "_" + CPLSPrintf ("%d", ++counter);
        if(counter == 10000)
            return ngsErrorCodes::UNEXPECTED_ERROR;
    }

    if(srcDs->type () == TABLE) {
        Table* srcTable = ngsDynamicCast(Table, srcDs);
        if(nullptr == srcTable) {
            return ngsErrorCodes::COPY_FAILED;
        }
        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition ();
        DatasetWPtr dstDsW = createDataset(name, srcDefinition);
        DatasetPtr dstDs = dstDsW.lock ();
        Table* dstTable = ngsDynamicCast(Table, dstDs);
        if(nullptr == dstTable) {
            return ngsErrorCodes::COPY_FAILED;
        }
        OGRFeatureDefn* const dstDefinition = dstTable->getDefinition ();

        CPLAssert (srcDefinition->GetFieldCount() == dstDefinition->GetFieldCount());
        // Create fields map. We expected equil count of fields
        vector<FieldMap> fieldMap;
        for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
            fieldMap.push_back ({i, i,
                                 dstDefinition->GetFieldDefn (i)->GetType ()});
        }

        return dstTable->copyRows(srcTable, fieldMap,
                                   progressFunc, progressArguments);
    }
    else if(srcDs->type () == FEATURESET) {
        FeatureDataset* srcTable = ngsDynamicCast(FeatureDataset, srcDs);
        if(nullptr == srcTable) {
            return ngsErrorCodes::COPY_FAILED;
        }
        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition ();
        if(progressFunc)
            progressFunc(0, CPLSPrintf ("Create new dataset '%s'",
                            srcDs->name ().c_str ()), progressArguments);
        DatasetWPtr dstDsW = createDataset(name, srcDefinition,
                                      srcTable->getSpatialReference (),
                                      srcTable->getGeometryType ());
        DatasetPtr dstDs = dstDsW.lock ();
        FeatureDataset* dstTable = ngsDynamicCast(FeatureDataset, dstDs);
        if(nullptr == dstTable) {
            return ngsErrorCodes::COPY_FAILED;
        }
        OGRFeatureDefn* const dstDefinition = dstTable->getDefinition ();

        CPLAssert (srcDefinition.GetFieldCount() == dstDefinition.GetFieldCount());
        // Create fields map. We expected equil count of fields
        vector<FieldMap> fieldMap;
        for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
            fieldMap.push_back ({i, i,
                                 dstDefinition->GetFieldDefn (i)->GetType ()});
        }

        return dstTable->copyRows(srcTable, fieldMap,
                                   progressFunc, progressArguments);
    }
    else { // TODO: raster and container support
        if(progressFunc)
            progressFunc(0, CPLSPrintf ("Dataset '%s' unsupported",
                            srcDs->name ().c_str ()), progressArguments);
        return ngsErrorCodes::UNSUPPORTED;
    }
    return ngsErrorCodes::SUCCESS;
}

int DatasetContainer::moveDataset(DatasetPtr srcDs, ngsProgressFunc progressFunc,
                                  void *progressArguments)
{
    int nResult = copyDataset (srcDs, progressFunc, progressArguments);
    if(nResult != ngsErrorCodes::SUCCESS)
        return nResult;
    return srcDs->destroy (progressFunc, progressArguments);
}

DatasetWPtr DatasetContainer::createDataset(const CPLString& name,
                                            OGRFeatureDefn * const definition,
                                            char **options)
{
    return createDataset (name, definition, nullptr, wkbNone, options);
}

DatasetWPtr DatasetContainer::createDataset(const CPLString &name,
                                            OGRFeatureDefn * const definition,
                                            const OGRSpatialReference &spatialRef,
                                            OGRwkbGeometryType type,
                                            char **options)
{
    OGRLayer *layerDest = m_DS->CreateLayer(name, spatialRef, type, options);
    if(poLayerDest == NULL)
    {
        wxString sErr = wxString::Format(_("Error create the output layer '%s'!"), sNewName.c_str());
        wxGISLogError(sErr, wxString::FromUTF8(CPLGetLastErrorMsg()), wxEmptyString, pTrackCancel);

        return NULL;
    }

    CPLString szNameField, szDescField;
    if (pFilter->GetType() == enumGISFeatureDataset)
    {
        if (pFilter->GetSubType() == enumVecDXF)
        {
            wxString sErr(_("DXF layer does not support arbitrary field creation."));
            wxLogWarning(sErr);
            CPLError(CE_Warning, CPLE_AppDefined, CPLString(sErr.ToUTF8()));
            if (pTrackCancel)
            {
                pTrackCancel->PutMessage(sErr, wxNOT_FOUND, enumGISMessageWarning);
            }
        }
        else
        {
            for (size_t i = 0; i < poFields->GetFieldCount(); ++i)
            {
                OGRFieldDefn *pField = poFields->GetFieldDefn(i);
                OGRFieldDefn oFieldDefn(pField);
                oFieldDefn.SetName(Transliterate(pField->GetNameRef()));
                if (pFilter->GetSubType() == enumVecKML || pFilter->GetSubType() == enumVecKMZ)
                {
                    OGRFieldType nType = pField->GetType();
                    if (OFTString == nType)
                    {
                        if (szNameField.empty())
                            szNameField = pField->GetNameRef();
                        if (szDescField.empty())
                            szDescField = pField->GetNameRef();
                    }
                }

                if (pFilter->GetSubType() == enumVecESRIShapefile && pField->GetType() == OFTTime)
                {
                    oFieldDefn.SetType(OFTString);
                    wxString sErr(_("Unsupported type for shape file - OFTTime. Change to OFTString."));
                    wxLogWarning(sErr);
                    CPLError(CE_Warning, CPLE_AppDefined, CPLString(sErr.ToUTF8()));
                    if (pTrackCancel)
                    {
                        pTrackCancel->PutMessage(sErr, wxNOT_FOUND, enumGISMessageWarning);
                    }
                }

                if (poLayerDest->CreateField(&oFieldDefn) != OGRERR_NONE)
                {
                    wxString sErr = wxString::Format(_("Error create the output layer '%s'!"), sNewName.c_str());
                    wxGISLogError(sErr, wxString::FromUTF8(CPLGetLastErrorMsg()), wxEmptyString, pTrackCancel);
                    return NULL;
                }
            }
            if (pFilter->GetSubType() == enumVecKML || pFilter->GetSubType() == enumVecKMZ)
            {
                if (!szNameField.empty())
                    CPLSetConfigOption("LIBKML_NAME_FIELD", szNameField);
                if (!szDescField.empty())
                    CPLSetConfigOption("LIBKML_DESCRIPTION_FIELD", szDescField);
                //CPLSetConfigOption( "LIBKML_TIMESTAMP_FIELD", "YES" );
            }
        }

        // TODO: this is hack withg spatial reference for GeoJSON driver
        wxGISFeatureDataset *pFeatureDataset = new wxGISFeatureDataset(szFullPath, pFilter->GetSubType(), poLayerDest, poDS, oSpatialRef);
        //pFeatureDataset->
        int nRefCount = poDS->Dereference();
        wxASSERT_MSG(nRefCount > 0, wxT("Reference counting error"));
        pFeatureDataset->SetEncoding(oEncoding);
        return wxStaticCast(pFeatureDataset, wxGISDataset);

    }

    if(nullptr != layer){
        Dataset* pDataset = nullptr;
        if( EQUAL(layer->GetGeometryColumn (), "")){
            pDataset = new Table(layer);
        }
        else {
            pDataset = new FeatureDataset(layer);
        }

        pDataset->setNotifyFunc (m_notifyFunc);
        dataset.reset(pDataset);
        if(pDataset)
            m_datasets[dataset->name ()] = dataset;
    }

}
