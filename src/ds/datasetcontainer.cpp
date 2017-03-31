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

#include "datasetcontainer.h"

// stl
#include <algorithm>
#include <array>

#include "featuredataset.h"
#include "catalog/folder.h"
#include "util/error.h"
#include "ngstore/catalog/filter.h"
#include "util/stringutil.h"

#define MAX_EQUAL_NAMES 10000
#define MAX_LOADTASK_COUNT 100

namespace ngs {

const static std::array<char, 22> forbiddenChars = {{':', '@', '#', '%', '^', '&', '*',
    '!', '$', '(', ')', '+', '-', '?', '=', '/', '\\', '"', '\'', '[', ']', ','}};

const static std::array<const char*, 124> forbiddenSQLFieldNames {{ "ABORT", "ACTION",
    "ADD", "AFTER", "ALL", "ALTER", "ANALYZE", "AND", "AS", "ASC", "ATTACH",
    "AUTOINCREMENT", "BEFORE", "BEGIN", "BETWEEN", "BY", "CASCADE", "CASE",
    "CAST", "CHECK", "COLLATE", "COLUMN", "COMMIT", "CONFLICT", "CONSTRAINT",
    "CREATE", "CROSS", "CURRENT_DATE", "CURRENT_TIME", "CURRENT_TIMESTAMP",
    "DATABASE", "DEFAULT", "DEFERRABLE", "DEFERRED", "DELETE", "DESC", "DETACH",
    "DISTINCT", "DROP", "EACH", "ELSE", "END", "ESCAPE", "EXCEPT", "EXCLUSIVE",
    "EXISTS", "EXPLAIN", "FAIL", "FOR", "FOREIGN", "FROM", "FULL", "GLOB",
    "GROUP", "HAVING", "IF", "IGNORE", "IMMEDIATE", "IN", "INDEX", "INDEXED",
    "INITIALLY", "INNER", "INSERT", "INSTEAD", "INTERSECT", "INTO", "IS", "ISNULL",
    "JOIN", "KEY", "LEFT", "LIKE", "LIMIT", "MATCH", "NATURAL", "NO", "NOT",
    "NOTNULL", "NULL", "OF", "OFFSET", "ON", "OR", "ORDER", "OUTER", "PLAN",
    "PRAGMA", "PRIMARY", "QUERY", "RAISE", "RECURSIVE", "REFERENCES", "REGEXP",
    "REINDEX", "RELEASE", "RENAME", "REPLACE", "RESTRICT", "RIGHT", "ROLLBACK",
    "ROW", "SAVEPOINT", "SELECT", "SET", "TABLE", "TEMP", "TEMPORARY", "THEN",
    "TO", "TRANSACTION", "TRIGGER", "UNION", "UNIQUE", "UPDATE", "USING",
    "VACUUM", "VALUES", "VIEW", "VIRTUAL", "WHEN", "WHERE", "WITH", "WITHOUT"}};

//void ngs::LoadingThread(void * store)
//{
//    DatasetContainer* dstDataset = static_cast<DatasetContainer*>(store);

//    for(LoadData &data : dstDataset->m_loadData) {
//        if(dstDataset->m_cancelLoad)
//            break;


//        if(data.status() != ngsErrorCodes::EC_PENDING) // all new tasks have pending state
//            continue;

//        CPLString srcName(CPLGetFilename( data.path() ));
//        data.setStatus( ngsErrorCodes::EC_IN_PROCESS );


//        if(!data.onProgress(0, CPLSPrintf ("Start loading '%s'", srcName.c_str ()))) {
//            data.setStatus( ngsErrorCodes::EC_CANCELED );
//                continue;
//        }

//        // 1. Open datasource
//        DatasetPtr srcDataset = Dataset::open (data.path(),
//                                        GDAL_OF_SHARED|GDAL_OF_READONLY, nullptr);
//        if(nullptr == srcDataset) {
//            CPLString errorMsg;
//            errorMsg.Printf ("Dataset '%s' open failed.", srcName.c_str ());
//            CPLErrorSetState(CE_Failure, CPLE_AppDefined, errorMsg);
//            data.setStatus (ngsErrorCodes::EC_OPEN_FAILED);
//            data.onProgress (2, errorMsg);
//            continue;
//        }

//        if(srcDataset->type () & ngsDatasetType (Container)) {
//            DatasetContainer* pDatasetContainer = static_cast<
//                    DatasetContainer*>(srcDataset.get ());
//            srcDataset = pDatasetContainer->getDataset (data.srcSubDatasetName());

//            if(nullptr == srcDataset) {
//                CPLString errorMsg;
//                errorMsg.Printf ("Dataset '%s' open failed.", srcName.c_str ());
//                CPLErrorSetState(CE_Failure, CPLE_AppDefined, errorMsg);
//                data.setStatus (ngsErrorCodes::EC_OPEN_FAILED);
//                data.onProgress (2, errorMsg);
//                continue;
//            }
//        }

//        // 2. Move or copy datasource to container
//        int res;
//        bool move = EQUAL(CSLFetchNameValueDef(data.options(), "LOAD_OP", "COPY"),
//                          "MOVE");
//        if(move && srcDataset->canDelete ()) {
//            res = dstDataset->moveDataset(srcDataset, data.getDestinationName (),
//                                    &data);
//        }
//        else {
//            res = dstDataset->copyDataset(srcDataset, data.getDestinationName (),
//                                    &data);
//        }
//        data.setStatus (static_cast<ngsErrorCodes>(res));
//        data.onProgress(2, CPLSPrintf ("Loading '%s' finished", srcName.c_str ()));
//    }

//    dstDataset->m_hLoadThread = nullptr;
//}

//------------------------------------------------------------------------------
// LoadData
//------------------------------------------------------------------------------
//LoadData::LoadData(unsigned int id, const CPLString &path,
//                   const CPLString &srcSubDatasetName,
//                   const CPLString &dstDatasetName, const char **options,
//                   ngsProgressFunc progressFunc, void *progressArguments) :
//    ProgressInfo(id, options, progressFunc, progressArguments), m_path(path),
//    m_srcSubDatasetName(srcSubDatasetName),
//    m_dstDatasetName(dstDatasetName)
//{

//}

//LoadData::~LoadData()
//{
//}

//LoadData::LoadData(const LoadData &data) : ProgressInfo(data)
//{
//    m_path = data.m_path;
//    m_srcSubDatasetName = data.m_srcSubDatasetName;
//    m_dstDatasetName = data.m_dstDatasetName;
//    m_newNames = data.m_newNames;
//}

//LoadData &LoadData::operator=(const LoadData &data)
//{
//    m_id = data.m_id;
//    m_path = data.m_path;
//    m_srcSubDatasetName = data.m_srcSubDatasetName;
//    m_dstDatasetName = data.m_dstDatasetName;
//    m_newNames = data.m_newNames;
//    m_options = CSLDuplicate (data.m_options);
//    m_progressFunc = data.m_progressFunc;
//    m_progressArguments = data.m_progressArguments;
//    m_status = data.m_status;
//    return *this;
//}

//const char *LoadData::path() const
//{
//    return m_path;
//}

//CPLString LoadData::srcSubDatasetName() const
//{
//    return m_srcSubDatasetName;
//}

//void LoadData::addNewName(const CPLString &name)
//{
//    if(m_newNames.empty ())
//        m_newNames += name;
//    else
//        m_newNames += ";" + name;
//}

//const char *LoadData::getNewNames() const
//{
//    return m_newNames;
//}

//const char *LoadData::getDestinationName() const
//{
//    return m_dstDatasetName;
//}

//------------------------------------------------------------------------------
// DatasetContainer
//------------------------------------------------------------------------------


DatasetContainer::DatasetContainer(ObjectContainer * const parent,
                                   const ngsCatalogObjectType type,
                                   const CPLString &name,
                                   const CPLString &path) :
    ObjectContainer(parent, type, name, path), m_DS(nullptr),
    m_hLoadThread(nullptr), m_cancelLoad(false), m_newTaskId(0)
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

bool DatasetContainer::isNameValid(const char* name) const
{
    return m_DS->GetLayerByName(name) == nullptr;
}

bool forbiddenChar (char c)
{
    return std::find(forbiddenChars.begin(), forbiddenChars.end(), c) !=
            forbiddenChars.end();
}

CPLString DatasetContainer::normalizeDatasetName(const CPLString &name) const
{
    // create unique name
    CPLString outName;
    if(name.empty ()) {
        outName = "new_dataset";
    }
    else {
        outName = translit(name);
        replace_if( outName.begin(), outName.end(), forbiddenChar, '_');
    }

    CPLString originName = outName;
    int nameCounter = 0;
    while(!isNameValid (outName)) {
        outName = originName + "_" + CPLSPrintf ("%d", ++nameCounter);
        if(nameCounter == MAX_EQUAL_NAMES) {
            return "";
        }
    }

    return outName;
}

CPLString DatasetContainer::normalizeFieldName(const CPLString &name) const
{
    CPLString out = translit (name); // TODO: add lang support, i.e. ru_RU)

    replace_if( out.begin(), out.end(), forbiddenChar, '_');

    if(isdigit (out[0]))
        out = "Fld_" + out;

    if(Filter::isDatabase (m_type)) {
        // check for sql forbidden names
        if(find(forbiddenSQLFieldNames.begin (), forbiddenSQLFieldNames.end(),
                out) != forbiddenSQLFieldNames.end()) {
            out += "_";
        }
    }

    return out;
}

const char *DatasetContainer::getOptions(enum ngsOptionTypes optionType) const
{
    if(nullptr == m_DS)
        return nullptr;
    GDALDriver *poDriver = m_DS->GetDriver ();
    switch (optionType) {
    case OT_CREATE_DATASOURCE:
        return poDriver->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);
    case OT_CREATE_LAYER:
        return poDriver->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST);
    case OT_CREATE_LAYER_FIELD:
        return poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES);
    case OT_CREATE_RASTER:
        return poDriver->GetMetadataItem(GDAL_DMD_CREATIONDATATYPES);
    case OT_OPEN:
        return poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST);
    case OT_LOAD:
        return nullptr;
    }
}

bool DatasetContainer::destroy()
{
    clear();
    GDALClose(m_DS);
    m_DS = nullptr;
    return Folder::deleteFile(m_path);
}

bool DatasetContainer::open(unsigned int openFlags, char **options)
{
    if(nullptr == m_path || EQUAL(m_path, "")) {
        return errorMessage(ngsErrorCodes::EC_OPEN_FAILED, _("The path is empty"));
    }

    // open
    CPLErrorReset();
    m_DS = static_cast<GDALDataset*>(GDALOpenEx( m_path, openFlags, nullptr,
                                                 options, nullptr));
    if(m_DS == nullptr) {
        return false; // Error message comes from GDALOpenEx
    }

    return true;
}

bool DatasetContainer::hasChildren()
{
    if(!isOpened()) {
        if(!open(GDAL_OF_SHARED|GDAL_OF_UPDATE, nullptr)) {
            return false;
        }
    }

    // fill vector layers and tables
    int i;
    for(i = 0; i < m_DS->GetLayerCount (); ++i){
        OGRLayer* layer = m_DS->GetLayer (i);
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            const char* layerName = layer->GetName();
            // layer->GetLayerDefn()->GetGeomFieldCount() == 0
            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new Table(this, CAT_TABLE_ANY, layerName, "")));
            }
            else {
                m_children.push_back(ObjectPtr(new FeatureClass(this, CAT_FC_ANY, layerName, "")));
            }
        }
    }

    // fill rasters
    char** subdatasetList = m_DS->GetMetadata ("SUBDATASETS");
    i = 0;
    size_t strLen = 0;
    const char* testStr = nullptr;
    char rasterName[255];
    while(subdatasetList[i] != nullptr) {
        ++i;
        testStr = subdatasetList[i];
        strLen = CPLStrnlen(testStr, 255);
        if(EQUAL(testStr + strLen - 4, "NAME")) {
            CPLStrlcpy(rasterName, testStr, strLen - 4);
            m_children.push_back(ObjectPtr(new Raster(this, CAT_RASTER_ANY, rasterName, "")));
        }
    }
    CSLDestroy(subdatasetList);

    m_childrenLoaded = true;

    return ObjectContainer::hasChildren();
}

//unsigned int DatasetContainer::loadDataset(const CPLString &name,
//                                           const CPLString& path, const
//                                  CPLString& subDatasetName, const char **options,
//                                  ngsProgressFunc progressFunc,
//                                  void* progressArguments)
//{
//    if(m_loadData.size () > MAX_LOADTASK_COUNT) {
//        std::vector<LoadData>(m_loadData.begin() + MAX_LOADTASK_COUNT - 10,
//                         m_loadData.end()).swap(m_loadData);
//    }

//    unsigned int id = ++m_newTaskId;
//    m_loadData.push_back ( LoadData(id, path, subDatasetName, name, options,
//                           progressFunc, progressArguments) );
//    if(nullptr == m_hLoadThread) {
//        m_hLoadThread = CPLCreateJoinableThread(LoadingThread, this);
//    }
//    return id;
//}

//int DatasetContainer::copyDataset(DatasetPtr srcDataset,
//                                  const CPLString &dstDatasetName,
//                                  LoadData *loadData)
//{
//    // create uniq name
//    CPLString name;
//    if(dstDatasetName.empty ())
//        name = normalizeDatasetName(srcDataset->name ());
//    else
//        name = normalizeDatasetName(dstDatasetName);

//    if(name.empty ())
//            return ngsErrorCodes::EC_UNEXPECTED_ERROR;

//    if(loadData)
//        loadData->onProgress (0, CPLSPrintf ("Copy dataset '%s' to '%s'",
//                        name.c_str (), m_name.c_str ()));

//    if(srcDataset->type () & ngsDatasetType(Table)) {
//        if(loadData)
//            loadData->addNewName (name);
//        Table* const srcTable = ngsDynamicCast(Table, srcDataset);
//        if(nullptr == srcTable) {
//            CPLString errorMsg;
//            errorMsg.Printf ("Source dataset '%s' report type TABLE, but it is not a table.",
//                     srcDataset->name ().c_str ());
//            return reportError (ngsErrorCodes::EC_COPY_FAILED, 0, errorMsg,
//                                loadData);
//        }
//        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition ();
//        DatasetPtr dstDataset = createDataset(name, srcDefinition, loadData);
//        Table* const dstTable = ngsDynamicCast(Table, dstDataset);
//        if(nullptr == dstTable) {
//            CPLString errorMsg;
//            errorMsg.Printf ("Destination dataset '%s' report type TABLE, but it is not a table.",
//                     srcDataset->name ().c_str ());
//            return reportError (ngsErrorCodes::EC_COPY_FAILED, 0, errorMsg,
//                                loadData);
//        }
//        OGRFeatureDefn* const dstDefinition = dstTable->getDefinition ();

//        CPLAssert (srcDefinition->GetFieldCount() ==
//                  dstDefinition->GetFieldCount());
//        // Create fields map. We expected equal count of fields
//        FieldMapPtr fieldMap(static_cast<unsigned long>(
//                                 dstDefinition->GetFieldCount()));
//        for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
//            fieldMap[i] = i;
//        }

//        return dstTable->copyRows(srcTable, fieldMap, loadData);
//    }
//    else if(srcDataset->type () & ngsDatasetType(Featureset)) {
//        FeatureDataset* const srcTable = ngsDynamicCast(FeatureDataset, srcDataset);
//        if(nullptr == srcTable) {
//            CPLString errorMsg;
//            errorMsg.Printf ("Source dataset '%s' report type FEATURESET, but it is not a feature class.",
//                     srcDataset->name ().c_str ());
//            return reportError (ngsErrorCodes::EC_COPY_FAILED, 0, errorMsg,
//                                loadData);
//        }
//        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition ();

//        // get output geometry type
//        std::vector<OGRwkbGeometryType> geometryTypes = getGeometryTypes(srcDataset);
//        for(OGRwkbGeometryType geometryType : geometryTypes) {
//            OGRwkbGeometryType filterGeomType = wkbUnknown;
//            CPLString newName = name;
//            if(geometryTypes.size () > 1) {
//                newName += "_" + FeatureDataset::getGeometryTypeName(
//                            geometryType, FeatureDataset::GeometryReportType::Simple);
//                if (OGR_GT_Flatten(geometryType) > wkbPolygon &&
//                    OGR_GT_Flatten(geometryType) < wkbGeometryCollection) {
//                   filterGeomType = static_cast<OGRwkbGeometryType>(
//                               geometryType - 3);
//               }
//            }

//            if(loadData)
//                loadData->addNewName (newName);
//            DatasetPtr dstDataset = createDataset(newName, srcDefinition,
//                                          srcTable->getSpatialReference (),
//                                          geometryType, loadData);
//            FeatureDataset* const dstTable = ngsDynamicCast(FeatureDataset, dstDataset);
//            if(nullptr == dstTable) {
//                CPLString errorMsg;
//                errorMsg.Printf ("Source dataset '%s' report type FEATURESET, but it is not a feature class.",
//                         srcDataset->name ().c_str ());
//                return reportError (ngsErrorCodes::EC_COPY_FAILED, 0, errorMsg,
//                                    loadData);
//            }
//            OGRFeatureDefn* const dstDefinition = dstTable->getDefinition ();

//            CPLAssert (srcDefinition->GetFieldCount() ==
//                       dstDefinition->GetFieldCount());
//            // Create fields map. We expected equal count of fields
//            FieldMapPtr fieldMap(static_cast<unsigned long>(
//                                     dstDefinition->GetFieldCount()));
//            for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
//                fieldMap[i] = i;
//            }

//            int nRes = dstTable->copyFeatures (srcTable, fieldMap, filterGeomType,
//                                               loadData);
//            if(nRes != ngsErrorCodes::EC_SUCCESS)
//                return nRes;
//        }
//    }
//    else { // TODO: raster and container support
//        CPLString errorMsg;
//        errorMsg.Printf ("Dataset '%s' unsupported",
//                            srcDataset->name ().c_str ());
//        return reportError (ngsErrorCodes::EC_UNSUPPORTED, 0, errorMsg,
//                            loadData);
//    }
//    return ngsErrorCodes::EC_SUCCESS;
//}

//int DatasetContainer::moveDataset(DatasetPtr srcDataset,
//                                  const CPLString &dstDatasetName,
//                                  LoadData *loadData)
//{
//    if(loadData)
//        loadData->onProgress (0, CPLSPrintf ("Move dataset '%s' to '%s'",
//                        srcDataset->name().c_str (), m_name.c_str ()));

//    int nResult = copyDataset (srcDataset, dstDatasetName, loadData);
//    if(nResult != ngsErrorCodes::EC_SUCCESS)
//        return nResult;
//    return srcDataset->destroy (loadData);
//}

//DatasetPtr DatasetContainer::createDataset(const CPLString& name,
//                                            OGRFeatureDefn* const definition,
//                                            ProgressInfo *progressInfo)
//{
//    return createDataset (name, definition, nullptr, wkbNone, progressInfo);
//}

//DatasetPtr DatasetContainer::createDataset(const CPLString &name,
//                                            OGRFeatureDefn* const definition,
//                                            const OGRSpatialReference *spatialRef,
//                                            OGRwkbGeometryType type,
//                                            ProgressInfo *progressInfo)
//{
//    char** options = nullptr;
//    if(progressInfo) {
//        progressInfo->onProgress (0, CPLSPrintf ("Create dataset '%s'", name.c_str ()));
//        options = progressInfo->options ();
//    }
//    DatasetPtr out;
//    OGRLayer *dstLayer = m_DS->CreateLayer(name,
//        const_cast<OGRSpatialReference*>(spatialRef), type, options);
//    if(dstLayer == nullptr) {
//        return out;
//    }

//    for (int i = 0; i < definition->GetFieldCount(); ++i) {
//        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
//        OGRFieldDefn dstField(srcField);

//        // TODO: add special behaviour for different drivers (see. ogr2ogr)

//        dstField.SetName(normalizeFieldName(srcField->GetNameRef()));
//        if (dstLayer->CreateField(&dstField) != OGRERR_NONE) {
//            return out;
//        }
//    }

//    Dataset* dstDataset = nullptr;
//    if( type == wkbNone) {
//        dstDataset = new Table(dstLayer);
//    }
//    else {
//        dstDataset = new FeatureDataset(dstLayer);
//    }

//    dstDataset->setNotifyFunc (m_notifyFunc);
//    dstDataset->m_DS = m_DS;
//    out.reset(dstDataset);
//    if(dstDataset)
//        m_datasets[out->name ()] = out;

//    return out;
//}

//ngsLoadTaskInfo DatasetContainer::getLoadTaskInfo(unsigned int taskId) const
//{
//    for(const LoadData& data : m_loadData) {
//        if(data.id () == taskId) {
//            return {data.getDestinationName (),
//                    data.getNewNames(),
//                    m_path.c_str (),
//                    data.status()};
//        }
//    }
//    return {NULL, NULL, NULL, ngsErrorCodes::EC_INVALID};
//}


}
