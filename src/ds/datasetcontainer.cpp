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
#include "stringutil.h"

#include <algorithm>
#include <array>

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
    while(!dstDataset->m_cancelLoad && !dstDataset->m_loadData.empty ()) {
        LoadData data = dstDataset->m_loadData.back ();
        dstDataset->m_loadData.pop_back ();

        if(data.progressFunc)
            if(data.progressFunc(0, CPLSPrintf ("Start loading '%s'",
                    CPLGetFilename(data.path)), data.progressArguments) == FALSE)
                continue;



        // 1. Open
        DatasetPtr srcDataset = Dataset::open (data.path, GDAL_OF_SHARED|GDAL_OF_READONLY,
                                       nullptr);
        if(nullptr == srcDataset) {
            CPLError(CE_Failure, CPLE_AppDefined, "Dataset '%s' open failed.",
                     CPLGetFilename(data.path));
            continue;
        }

        if(srcDataset->type () & ngsDatasetType (Container)) {
            DatasetContainer* pDatasetContainer = static_cast<
                    DatasetContainer*>(srcDataset.get ());
            srcDataset = pDatasetContainer->getDataset (data.srcSubDatasetName);
        }

        // 2. Move or cope ds to container
        if(data.move && srcDataset->canDelete ()) {
            dstDataset->moveDataset(srcDataset, data.dstDatasetName,
                                    data.skipType, data.progressFunc,
                                    data.progressArguments);
        }
        else {
            dstDataset->copyDataset(srcDataset, data.dstDatasetName,
                                    data.skipType, data.progressFunc,
                                    data.progressArguments);
        }
    }

    dstDataset->m_hLoadThread = nullptr;
}

//------------------------------------------------------------------------------
// DatasetContainer
//------------------------------------------------------------------------------


DatasetContainer::DatasetContainer() : Dataset(), m_hLoadThread(nullptr),
    m_cancelLoad(false)
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
        srcFD->setIgnoredFields ((const char**)fieldsPtr.get());
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

int DatasetContainer::loadDataset(const CPLString &name, const CPLString& path, const
                                  CPLString& subDatasetName, bool move,
                                  unsigned int skipType,
                                  ngsProgressFunc progressFunc,
                                  void* progressArguments)
{
    m_loadData.push_back ({path, subDatasetName, name, move, skipType,
                           progressFunc, progressArguments});
    if(nullptr == m_hLoadThread) {
        m_hLoadThread = CPLCreateJoinableThread(LoadingThread, this);
    }
    return ngsErrorCodes::SUCCESS;
}

int DatasetContainer::copyDataset(DatasetPtr srcDataset,
                                  const CPLString &dstDatasetName,
                                  unsigned int skipGeometryFlags,
                                  ngsProgressFunc progressFunc,
                                  void *progressArguments)
{
    // create uniq name
    CPLString name;
    if(dstDatasetName.empty ())
        name = srcDataset->name ();
    else
        name = dstDatasetName;
    if(name.empty ())
        name = "new_dataset";
    CPLString newName = name;
    int counter = 0;
    while(!isNameValid (newName)) {
        newName = name + "_" + CPLSPrintf ("%d", ++counter);
        if(counter == 10000)
            return ngsErrorCodes::UNEXPECTED_ERROR;
    }
    if(progressFunc)
        progressFunc(0, CPLSPrintf ("Copy dataset '%s' to '%s'",
                        name.c_str (), m_name.c_str ()),
                     progressArguments);

    if(srcDataset->type () & ngsDatasetType(Table)) {
        Table* const srcTable = ngsDynamicCast(Table, srcDataset);
        if(nullptr == srcTable) {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Source dataset '%s' report type TABLE, but it is not a table.",
                     srcDataset->name ().c_str ());
            return ngsErrorCodes::COPY_FAILED;
        }
        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition ();
        DatasetPtr dstDataset = createDataset(newName, srcDefinition, nullptr,
                                           progressFunc, progressArguments);
        Table* const dstTable = ngsDynamicCast(Table, dstDataset);
        if(nullptr == dstTable) {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Destination dataset '%s' report type TABLE, but it is not a table.",
                     srcDataset->name ().c_str ());
            return ngsErrorCodes::COPY_FAILED;
        }
        OGRFeatureDefn* const dstDefinition = dstTable->getDefinition ();

        CPLAssert (srcDefinition->GetFieldCount() == dstDefinition->GetFieldCount());
        // Create fields map. We expected equal count of fields
        FieldMapPtr fieldMap(static_cast<unsigned long>(
                                 dstDefinition->GetFieldCount()));
        for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
            fieldMap[i] = i;
        }

        return dstTable->copyRows(srcTable, fieldMap,
                                   progressFunc, progressArguments);
    }
    else if(srcDataset->type () & ngsDatasetType(Featureset)) {
        FeatureDataset* const srcTable = ngsDynamicCast(FeatureDataset, srcDataset);
        if(nullptr == srcTable) {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Source dataset '%s' report type FEATURESET, but it is not a "
                     "feature class.",
                     srcDataset->name ().c_str ());

            return ngsErrorCodes::COPY_FAILED;
        }
        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition ();

        // get output geometry type
        vector<OGRwkbGeometryType> geometryTypes = getGeometryTypes(srcDataset);
        for(OGRwkbGeometryType geometryType : geometryTypes) {
            OGRwkbGeometryType filterGeomType = wkbUnknown;
            if(geometryTypes.size () > 1) {
                newName += "_" + FeatureDataset::getGeometryTypeName(
                            geometryType, FeatureDataset::GeometryReportType::Simple);
                if (OGR_GT_Flatten(geometryType) > wkbPolygon &&
                    OGR_GT_Flatten(geometryType) < wkbGeometryCollection) {
                   filterGeomType = static_cast<OGRwkbGeometryType>(
                               geometryType - 3);
               }
            }
            DatasetPtr dstDataset = createDataset(newName, srcDefinition,
                                          srcTable->getSpatialReference (),
                                          geometryType, nullptr,
                                          progressFunc, progressArguments);
            FeatureDataset* const dstTable = ngsDynamicCast(FeatureDataset, dstDataset);
            if(nullptr == dstTable) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Source dataset '%s' report type FEATURESET, but it is not a "
                         "feature class.",
                         srcDataset->name ().c_str ());
                return ngsErrorCodes::COPY_FAILED;
            }
            OGRFeatureDefn* const dstDefinition = dstTable->getDefinition ();

            CPLAssert (srcDefinition->GetFieldCount() !=
                       dstDefinition->GetFieldCount());
            // Create fields map. We expected equal count of fields
            FieldMapPtr fieldMap(static_cast<unsigned long>(
                                     dstDefinition->GetFieldCount()));
            for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
                fieldMap[i] = i;
            }

            int nRes = dstTable->copyFeatures (srcTable, fieldMap, filterGeomType,
                                              skipGeometryFlags, progressFunc,
                                              progressArguments);
            if(nRes != ngsErrorCodes::SUCCESS)
                return nRes;
        }
    }
    else { // TODO: raster and container support

        CPLError(CE_Failure, CPLE_AppDefined, "Dataset '%s' unsupported",
                            srcDataset->name ().c_str ());
        return ngsErrorCodes::UNSUPPORTED;
    }
    return ngsErrorCodes::SUCCESS;
}

int DatasetContainer::moveDataset(DatasetPtr srcDataset,
                                  const CPLString &dstDatasetName,
                                  unsigned int skipGeometryFlags,
                                  ngsProgressFunc progressFunc,
                                  void *progressArguments)
{
    if(progressFunc)
        progressFunc(0, CPLSPrintf ("Move dataset '%s' to '%s'",
                        srcDataset->name().c_str (), m_name.c_str ()),
                     progressArguments);

    int nResult = copyDataset (srcDataset, dstDatasetName, skipGeometryFlags,
                               progressFunc, progressArguments);
    if(nResult != ngsErrorCodes::SUCCESS)
        return nResult;
    return srcDataset->destroy (progressFunc, progressArguments);
}

DatasetPtr DatasetContainer::createDataset(const CPLString& name,
                                            OGRFeatureDefn* const definition,
                                            char **options,
                                            ngsProgressFunc progressFunc,
                                            void* progressArguments)
{
    return createDataset (name, definition, nullptr, wkbNone, options,
                          progressFunc, progressArguments);
}

DatasetPtr DatasetContainer::createDataset(const CPLString &name,
                                            OGRFeatureDefn* const definition,
                                            const OGRSpatialReference *spatialRef,
                                            OGRwkbGeometryType type,
                                            char **options,
                                            ngsProgressFunc progressFunc,
                                            void *progressArguments)
{
    if(progressFunc)
        progressFunc(0, CPLSPrintf ("Create dataset '%s'", name.c_str ()),
                     progressArguments);
    DatasetPtr out;
    OGRLayer *dstLayer = m_DS->CreateLayer(name,
                                            const_cast<OGRSpatialReference*>(
                                                spatialRef), type, options);
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
