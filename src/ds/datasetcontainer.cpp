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
#include <algorithm>
#include <array>
#include "datasetcontainer.h"
#include "featuredataset.h"
#include "util/stringutil.h"

#define MAX_EQUAL_NAMES 10000
#define MAX_LOADTASK_COUNT 100

using namespace ngs;

const static array<char, 22> forbiddenChars = {{':', '@', '#', '%', '^', '&', '*',
    '!', '$', '(', ')', '+', '-', '?', '=', '/', '\\', '"', '\'', '[', ']', ','}};

const static array<const char*, 124> forbiddenSQLFieldNames {{ "ABORT", "ACTION",
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

void ngs::LoadingThread(void * store)
{
    DatasetContainer* dstDataset = static_cast<DatasetContainer*>(store);

    for(LoadData &data : dstDataset->m_loadData) {
        if(dstDataset->m_cancelLoad)
            break;


        if(data.status() != ngsErrorCodes::EC_PENDING) // all new tasks have pending state
            continue;

        CPLString srcName(CPLGetFilename( data.path() ));
        data.setStatus( ngsErrorCodes::EC_IN_PROCESS );


        if(!data.onProgress(0, CPLSPrintf ("Start loading '%s'", srcName.c_str ()))) {
            data.setStatus( ngsErrorCodes::EC_CANCELED );
                continue;
        }

        // 1. Open datasource
        DatasetPtr srcDataset = Dataset::open (data.path(),
                                        GDAL_OF_SHARED|GDAL_OF_READONLY, nullptr);
        if(nullptr == srcDataset) {
            CPLString errorMsg;
            errorMsg.Printf ("Dataset '%s' open failed.", srcName.c_str ());
            CPLErrorSetState(CE_Failure, CPLE_AppDefined, errorMsg);
            data.setStatus (ngsErrorCodes::EC_OPEN_FAILED);
            data.onProgress (2, errorMsg);
            continue;
        }

        if(srcDataset->type () & ngsDatasetType (Container)) {
            DatasetContainer* pDatasetContainer = static_cast<
                    DatasetContainer*>(srcDataset.get ());
            srcDataset = pDatasetContainer->getDataset (data.srcSubDatasetName());

            if(nullptr == srcDataset) {
                CPLString errorMsg;
                errorMsg.Printf ("Dataset '%s' open failed.", srcName.c_str ());
                CPLErrorSetState(CE_Failure, CPLE_AppDefined, errorMsg);
                data.setStatus (ngsErrorCodes::EC_OPEN_FAILED);
                data.onProgress (2, errorMsg);
                continue;
            }
        }

        // 2. Move or copy datasource to container
        int res;
        bool move = EQUAL(CSLFetchNameValueDef(data.options(), "LOAD_OP", "COPY"),
                          "MOVE");
        if(move && srcDataset->canDelete ()) {
            res = dstDataset->moveDataset(srcDataset, data.getDestinationName (),
                                    &data);
        }
        else {
            res = dstDataset->copyDataset(srcDataset, data.getDestinationName (),
                                    &data);
        }
        data.setStatus (static_cast<ngsErrorCodes>(res));
        data.onProgress(2, CPLSPrintf ("Loading '%s' finished", srcName.c_str ()));
    }

    dstDataset->m_hLoadThread = nullptr;
}

//------------------------------------------------------------------------------
// LoadData
//------------------------------------------------------------------------------
LoadData::LoadData(unsigned int id, const CPLString &path,
                   const CPLString &srcSubDatasetName,
                   const CPLString &dstDatasetName, const char **options,
                   ngsProgressFunc progressFunc, void *progressArguments) :
    ProgressInfo(id, options, progressFunc, progressArguments), m_path(path),
    m_srcSubDatasetName(srcSubDatasetName),
    m_dstDatasetName(dstDatasetName)
{

}

LoadData::~LoadData()
{
}

LoadData::LoadData(const LoadData &data) : ProgressInfo(data)
{
    m_path = data.m_path;
    m_srcSubDatasetName = data.m_srcSubDatasetName;
    m_dstDatasetName = data.m_dstDatasetName;
    m_newNames = data.m_newNames;
}

LoadData &LoadData::operator=(const LoadData &data)
{
    m_id = data.m_id;
    m_path = data.m_path;
    m_srcSubDatasetName = data.m_srcSubDatasetName;
    m_dstDatasetName = data.m_dstDatasetName;
    m_newNames = data.m_newNames;
    m_options = CSLDuplicate (data.m_options);
    m_progressFunc = data.m_progressFunc;
    m_progressArguments = data.m_progressArguments;
    m_status = data.m_status;
    return *this;
}

const char *LoadData::path() const
{
    return m_path;
}

CPLString LoadData::srcSubDatasetName() const
{
    return m_srcSubDatasetName;
}

void LoadData::addNewName(const CPLString &name)
{
    if(m_newNames.empty ())
        m_newNames += name;
    else
        m_newNames += ";" + name;
}

const char *LoadData::getNewNames() const
{
    return m_newNames;
}

const char *LoadData::getDestinationName() const
{
    return m_dstDatasetName;
}

//------------------------------------------------------------------------------
// DatasetContainer
//------------------------------------------------------------------------------


DatasetContainer::DatasetContainer() : Dataset(), m_hLoadThread(nullptr),
    m_cancelLoad(false), m_newTaskId(0)
{
    m_type = ngsDatasetType (Container);
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

DatasetPtr DatasetContainer::getDataset(const CPLString &name)
{
    DatasetPtr outDataset;
    auto it = m_datasets.find (name);
    if( it != m_datasets.end ()){
        if(!it->second->isDeleted ()){
            return it->second;
        }
        else{
            return outDataset;
        }
    }

    OGRLayer* layer = m_DS->GetLayerByName (name);
    if(nullptr != layer){
        Dataset* dataset = nullptr;
        if( layer->GetLayerDefn ()->GetGeomFieldCount () == 0 ){
            dataset = new Table(layer);
        }
        else {
            dataset = new FeatureDataset(layer);
        }

        dataset->setNotifyFunc (m_notifyFunc);
        dataset->m_DS = m_DS;
        dataset->m_path = m_path + "/" + name;
        outDataset.reset(dataset);
        if(dataset)
            m_datasets[outDataset->name ()] = outDataset;
    }

    return outDataset;
}

// TODO: getRaster(int index)
// TODO: getRaster(const CPLString &name)
DatasetPtr DatasetContainer::getDataset(int index)
{
    for(int i = 0; i < m_DS->GetLayerCount (); ++i){
        OGRLayer* layer = m_DS->GetLayer (i);
        if( i == index)
            return getDataset (layer->GetName());
    }
    return DatasetPtr();
}

bool DatasetContainer::isNameValid(const CPLString &name) const
{
    return m_DS->GetLayerByName (name) == nullptr;
}

bool forbiddenChar (char c)
{
    return find(forbiddenChars.begin(), forbiddenChars.end(), c) !=
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

    if(isDatabase ()) {
        // check for sql forbidden names
        if(find(forbiddenSQLFieldNames.begin (), forbiddenSQLFieldNames.end(),
                out) != forbiddenSQLFieldNames.end()) {
            out += "_";
        }
    }

    return out;
}

bool DatasetContainer::isDatabase() const
{
    if(nullptr == m_DS)
        return false;
    CPLString driverName = m_DS->GetDriver ()->GetDescription ();
    return driverName == "GPKG" || driverName == "SQLite" ||
            driverName == "PostgreSQL" || driverName == "OpenFileGDB" ||
            driverName == "MySQL" || driverName == "MongoDB" ||
            driverName == "CartoDB" || driverName == "PostGISRaster" ||
            driverName == "GNMDatabase";

}

vector<OGRwkbGeometryType> DatasetContainer::getGeometryTypes(DatasetPtr srcDataset)
{
    vector<OGRwkbGeometryType> out;
    FeatureDataset* const srcFD = ngsDynamicCast(FeatureDataset, srcDataset);
    if(nullptr == srcFD)
        return out;
    OGRwkbGeometryType geomType = srcFD->getGeometryType ();
    if (OGR_GT_Flatten(geomType) == wkbUnknown ||
            OGR_GT_Flatten(geomType) == wkbGeometryCollection) {

        char** ignoreFields = nullptr;
        unique_ptr<char*, void(*)(char**)> fieldsPtr(ignoreFields, CSLDestroy);
        OGRFeatureDefn* defn = srcFD->getDefinition ();
        for(int i = 0; i < defn->GetFieldCount (); ++i) {
            OGRFieldDefn *fld = defn->GetFieldDefn (i);
            ignoreFields = CSLAddString (ignoreFields, fld->GetNameRef ());
        }
        ignoreFields = CSLAddString (ignoreFields, "OGR_STYLE");
        srcFD->setIgnoredFields (const_cast<const char**>(fieldsPtr.get()));
        srcFD->reset ();
        map<OGRwkbGeometryType, int> counts;
        FeaturePtr feature;
        while((feature = srcFD->nextFeature ()) != nullptr) {
            OGRGeometry * geom = feature->GetGeometryRef ();
            if (nullptr != geom) {
                OGRwkbGeometryType geomType = geom->getGeometryType ();
                counts[OGR_GT_Flatten(geomType)] += 1;
            }
        }
        srcFD->setIgnoredFields (nullptr);

        if(counts[wkbPoint] > 0) {
            if(counts[wkbMultiPoint] > 0)
                out.push_back (wkbMultiPoint);
            else
                out.push_back (wkbPoint);
        }
        else if(counts[wkbLineString] > 0) {
            if(counts[wkbMultiLineString] > 0)
                out.push_back (wkbMultiLineString);
            else
                out.push_back (wkbLineString);
        }
        else if(counts[wkbPolygon] > 0) {
            if(counts[wkbMultiPolygon] > 0)
                out.push_back (wkbMultiPolygon);
            else
                out.push_back (wkbPolygon);
        }
    }
    else {
        out.push_back (geomType);
    }
    return out;
}

unsigned int DatasetContainer::loadDataset(const CPLString &name,
                                           const CPLString& path, const
                                  CPLString& subDatasetName, const char **options,
                                  ngsProgressFunc progressFunc,
                                  void* progressArguments)
{
    if(m_loadData.size () > MAX_LOADTASK_COUNT) {
        vector<LoadData>(m_loadData.begin() + MAX_LOADTASK_COUNT - 10,
                         m_loadData.end()).swap(m_loadData);
    }

    unsigned int id = ++m_newTaskId;
    m_loadData.push_back ( LoadData(id, path, subDatasetName, name, options,
                           progressFunc, progressArguments) );
    if(nullptr == m_hLoadThread) {
        m_hLoadThread = CPLCreateJoinableThread(LoadingThread, this);
    }
    return id;
}

int DatasetContainer::copyDataset(DatasetPtr srcDataset,
                                  const CPLString &dstDatasetName,
                                  LoadData *loadData)
{
    // create uniq name
    CPLString name;
    if(dstDatasetName.empty ())
        name = normalizeDatasetName(srcDataset->name ());
    else
        name = normalizeDatasetName(dstDatasetName);

    if(name.empty ())
            return ngsErrorCodes::EC_UNEXPECTED_ERROR;

    if(loadData)
        loadData->onProgress (0, CPLSPrintf ("Copy dataset '%s' to '%s'",
                        name.c_str (), m_name.c_str ()));

    if(srcDataset->type () & ngsDatasetType(Table)) {
        if(loadData)
            loadData->addNewName (name);
        Table* const srcTable = ngsDynamicCast(Table, srcDataset);
        if(nullptr == srcTable) {
            CPLString errorMsg;
            errorMsg.Printf ("Source dataset '%s' report type TABLE, but it is not a table.",
                     srcDataset->name ().c_str ());
            return reportError (ngsErrorCodes::EC_COPY_FAILED, 0, errorMsg,
                                loadData);
        }
        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition ();
        DatasetPtr dstDataset = createDataset(name, srcDefinition, loadData);
        Table* const dstTable = ngsDynamicCast(Table, dstDataset);
        if(nullptr == dstTable) {
            CPLString errorMsg;
            errorMsg.Printf ("Destination dataset '%s' report type TABLE, but it is not a table.",
                     srcDataset->name ().c_str ());
            return reportError (ngsErrorCodes::EC_COPY_FAILED, 0, errorMsg,
                                loadData);
        }
        OGRFeatureDefn* const dstDefinition = dstTable->getDefinition ();

        CPLAssert (srcDefinition->GetFieldCount() ==
                  dstDefinition->GetFieldCount());
        // Create fields map. We expected equal count of fields
        FieldMapPtr fieldMap(static_cast<unsigned long>(
                                 dstDefinition->GetFieldCount()));
        for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
            fieldMap[i] = i;
        }

        return dstTable->copyRows(srcTable, fieldMap, loadData);
    }
    else if(srcDataset->type () & ngsDatasetType(Featureset)) {
        FeatureDataset* const srcTable = ngsDynamicCast(FeatureDataset, srcDataset);
        if(nullptr == srcTable) {
            CPLString errorMsg;
            errorMsg.Printf ("Source dataset '%s' report type FEATURESET, but it is not a feature class.",
                     srcDataset->name ().c_str ());
            return reportError (ngsErrorCodes::EC_COPY_FAILED, 0, errorMsg,
                                loadData);
        }
        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition ();

        // get output geometry type
        vector<OGRwkbGeometryType> geometryTypes = getGeometryTypes(srcDataset);
        for(OGRwkbGeometryType geometryType : geometryTypes) {
            OGRwkbGeometryType filterGeomType = wkbUnknown;
            CPLString newName = name;
            if(geometryTypes.size () > 1) {
                newName += "_" + FeatureDataset::getGeometryTypeName(
                            geometryType, FeatureDataset::GeometryReportType::Simple);
                if (OGR_GT_Flatten(geometryType) > wkbPolygon &&
                    OGR_GT_Flatten(geometryType) < wkbGeometryCollection) {
                   filterGeomType = static_cast<OGRwkbGeometryType>(
                               geometryType - 3);
               }
            }

            if(loadData)
                loadData->addNewName (newName);
            DatasetPtr dstDataset = createDataset(newName, srcDefinition,
                                          srcTable->getSpatialReference (),
                                          geometryType, loadData);
            FeatureDataset* const dstTable = ngsDynamicCast(FeatureDataset, dstDataset);
            if(nullptr == dstTable) {
                CPLString errorMsg;
                errorMsg.Printf ("Source dataset '%s' report type FEATURESET, but it is not a feature class.",
                         srcDataset->name ().c_str ());
                return reportError (ngsErrorCodes::EC_COPY_FAILED, 0, errorMsg,
                                    loadData);
            }
            OGRFeatureDefn* const dstDefinition = dstTable->getDefinition ();

            CPLAssert (srcDefinition->GetFieldCount() ==
                       dstDefinition->GetFieldCount());
            // Create fields map. We expected equal count of fields
            FieldMapPtr fieldMap(static_cast<unsigned long>(
                                     dstDefinition->GetFieldCount()));
            for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
                fieldMap[i] = i;
            }

            int nRes = dstTable->copyFeatures (srcTable, fieldMap, filterGeomType,
                                               loadData);
            if(nRes != ngsErrorCodes::EC_SUCCESS)
                return nRes;
        }
    }
    else { // TODO: raster and container support
        CPLString errorMsg;
        errorMsg.Printf ("Dataset '%s' unsupported",
                            srcDataset->name ().c_str ());
        return reportError (ngsErrorCodes::EC_UNSUPPORTED, 0, errorMsg,
                            loadData);
    }
    return ngsErrorCodes::EC_SUCCESS;
}

int DatasetContainer::moveDataset(DatasetPtr srcDataset,
                                  const CPLString &dstDatasetName,
                                  LoadData *loadData)
{
    if(loadData)
        loadData->onProgress (0, CPLSPrintf ("Move dataset '%s' to '%s'",
                        srcDataset->name().c_str (), m_name.c_str ()));

    int nResult = copyDataset (srcDataset, dstDatasetName, loadData);
    if(nResult != ngsErrorCodes::EC_SUCCESS)
        return nResult;
    return srcDataset->destroy (loadData);
}

DatasetPtr DatasetContainer::createDataset(const CPLString& name,
                                            OGRFeatureDefn* const definition,
                                            ProgressInfo *progressInfo)
{
    return createDataset (name, definition, nullptr, wkbNone, progressInfo);
}

DatasetPtr DatasetContainer::createDataset(const CPLString &name,
                                            OGRFeatureDefn* const definition,
                                            const OGRSpatialReference *spatialRef,
                                            OGRwkbGeometryType type,
                                            ProgressInfo *progressInfo)
{
    char** options = nullptr;
    if(progressInfo) {
        progressInfo->onProgress (0, CPLSPrintf ("Create dataset '%s'", name.c_str ()));
        options = progressInfo->options ();
    }
    DatasetPtr out;
    OGRLayer *dstLayer = m_DS->CreateLayer(name,
        const_cast<OGRSpatialReference*>(spatialRef), type, options);
    if(dstLayer == nullptr) {
        return out;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) {
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        // TODO: add special behaviour for different drivers (see. ogr2ogr)

        dstField.SetName(normalizeFieldName(srcField->GetNameRef()));
        if (dstLayer->CreateField(&dstField) != OGRERR_NONE) {
            return out;
        }
    }

    Dataset* dstDataset = nullptr;
    if( type == wkbNone) {
        dstDataset = new Table(dstLayer);
    }
    else {
        dstDataset = new FeatureDataset(dstLayer);
    }

    dstDataset->setNotifyFunc (m_notifyFunc);
    dstDataset->m_DS = m_DS;
    out.reset(dstDataset);
    if(dstDataset)
        m_datasets[out->name ()] = out;

    return out;
}

ngsLoadTaskInfo DatasetContainer::getLoadTaskInfo(unsigned int taskId) const
{
    for(const LoadData& data : m_loadData) {
        if(data.id () == taskId) {
            return {data.getDestinationName (),
                    data.getNewNames(),
                    m_path.c_str (),
                    data.status()};
        }
    }
    return {NULL, NULL, NULL, ngsErrorCodes::EC_INVALID};
}

const char *DatasetContainer::getOptions(ngsDataStoreOptionsTypes optionType) const
{
    if(nullptr == m_DS)
        return nullptr;
    GDALDriver *poDriver = m_DS->GetDriver ();
    switch (optionType) {
    case OT_CREATE_DATASOURCE:
    case OT_OPEN:
    case OT_LOAD:
        return nullptr;
    case OT_CREATE_DATASET:
        return poDriver->GetMetadataItem (GDAL_DS_LAYER_CREATIONOPTIONLIST);
    }
}

int DatasetContainer::destroy(ProgressInfo *processInfo)
{
    m_datasets.clear ();
    return Dataset::destroy (processInfo);
}
