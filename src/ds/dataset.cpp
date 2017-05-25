/******************************************************************************
*  Project: NextGIS ...
*  Purpose: Application for ...
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2012-2016 NextGIS, info@nextgis.ru
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "dataset.h"

// stl
#include <algorithm>
#include <array>

#include "featureclass.h"
#include "raster.h"
#include "table.h"
#include "catalog/file.h"
#include "catalog/folder.h"
#include "ngstore/api.h"
#include "ngstore/catalog/filter.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/stringutil.h"

namespace ngs {

constexpr std::array<char, 22> forbiddenChars = {{':', '@', '#', '%', '^', '&', '*',
    '!', '$', '(', ')', '+', '-', '?', '=', '/', '\\', '"', '\'', '[', ']', ','}};

constexpr std::array<const char*, 124> forbiddenSQLFieldNames {{ "ABORT", "ACTION",
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

constexpr unsigned short MAX_EQUAL_NAMES = 10000;

//------------------------------------------------------------------------------
// GDALDatasetPtr
//------------------------------------------------------------------------------

GDALDatasetPtr::GDALDatasetPtr(GDALDataset* ds) :
    shared_ptr(ds, GDALClose)
{
}

GDALDatasetPtr::GDALDatasetPtr() :
    shared_ptr(nullptr, GDALClose)
{

}

GDALDatasetPtr::GDALDatasetPtr(const GDALDatasetPtr &ds) : shared_ptr(ds)
{
}

GDALDatasetPtr& GDALDatasetPtr::operator=(GDALDataset* ds) {
    reset(ds);
    return *this;
}

GDALDatasetPtr::operator GDALDataset *() const
{
    return get();
}

//------------------------------------------------------------------------------
// DatasetBase
//------------------------------------------------------------------------------
DatasetBase::DatasetBase() :
    m_DS(nullptr)
{

}

DatasetBase::~DatasetBase()
{
    GDALClose(m_DS);
    m_DS = nullptr;
}

bool DatasetBase::isReadOnly() const
{
     if(m_DS == nullptr)
         return true;
     return m_DS->GetAccess() == GA_ReadOnly;
}

const char *DatasetBase::getOptions(const enum ngsCatalogObjectType type,
                                    ngsOptionType optionType) const
{
    GDALDriver *poDriver = Filter::getGDALDriver(type);
    switch (optionType) {
    case OT_CREATE_DATASOURCE:
        if(nullptr == poDriver)
            return "";
        return poDriver->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);
    case OT_CREATE_LAYER:
        if(nullptr == poDriver)
            return "";
        return poDriver->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST);
    case OT_CREATE_LAYER_FIELD:
        if(nullptr == poDriver)
            return "";
        return poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES);
    case OT_CREATE_RASTER:
        if(nullptr == poDriver)
            return "";
        return poDriver->GetMetadataItem(GDAL_DMD_CREATIONDATATYPES);
    case OT_OPEN:
        if(nullptr == poDriver)
            return "";
        return poDriver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST);
    case OT_LOAD:
        return "";
    }
}

bool DatasetBase::open(const char* path, unsigned int openFlags,
                       const Options &options)
{
    if(nullptr == path || EQUAL(path, "")) {
        return errorMessage(ngsCode::COD_OPEN_FAILED, _("The path is empty"));
    }

    // NOTE: VALIDATE_OPEN_OPTIONS can be set to NO to avoid warnings

    CPLErrorReset();
    auto openOptions = options.getOptions();
    m_DS = static_cast<GDALDataset*>(GDALOpenEx( path, openFlags, nullptr,
                                                 openOptions.get(), nullptr));

    if(m_DS == nullptr) {
        errorMessage(CPLGetLastErrorMsg());
        if(openFlags & GDAL_OF_UPDATE) {
            // Try to open read-only
            openFlags &= static_cast<unsigned int>(~GDAL_OF_UPDATE);
            openFlags |= GDAL_OF_READONLY;
            m_DS = static_cast<GDALDataset*>(GDALOpenEx( path, openFlags,
                                          nullptr, openOptions.get(), nullptr));
            if(nullptr == m_DS) {
                errorMessage(CPLGetLastErrorMsg());
                return false; // Error message comes from GDALOpenEx
            }            
        }
        else {
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// Dataset
//------------------------------------------------------------------------------
constexpr const char* OVR_EXT = "ngvt";

Dataset::Dataset(ObjectContainer * const parent,
                 const enum ngsCatalogObjectType type,
                 const CPLString &name,
                 const CPLString &path) :
    ObjectContainer(parent, type, name, path),
    DatasetBase(),
    m_ovrDS(nullptr)
{
}

Dataset::~Dataset()
{
    GDALClose(m_ovrDS);
    m_ovrDS = nullptr;
}

FeatureClass *Dataset::createFeatureClass(const CPLString &name,
                                          OGRFeatureDefn * const definition,
                                          const OGRSpatialReference *spatialRef,
                                          OGRwkbGeometryType type,
                                          const Options &options,
                                          const Progress &progress)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened."));
        return nullptr;
    }

    OGRLayer *layer = m_DS->CreateLayer(name,
                    const_cast<OGRSpatialReference*>(spatialRef), type,
                                        options.getOptions().get());
    if(layer == nullptr) {
        errorMessage(CPLGetLastErrorMsg());
        return nullptr;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) {
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        CPLString newFieldName = normalizeFieldName(srcField->GetNameRef());
        if(!EQUAL(newFieldName, srcField->GetNameRef())) {
            progress.onProgress(ngsCode::COD_WARNING, 0.0,
                                _("Field %s of source table was renamed to %s in destination tables"),
                                srcField->GetNameRef(), newFieldName.c_str());
        }

        dstField.SetName(newFieldName);
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(CPLGetLastErrorMsg());
            return nullptr;
        }
    }

    FeatureClass* out = new FeatureClass(layer, this,
                                         ngsCatalogObjectType::CAT_FC_ANY, name);

    if(options.getBoolOption("CREATE_OVERVIEWS_TABLE", false) ||
            options.getBoolOption("CREATE_OVERVIEWS", false)) {
        out->createOverviews(progress, options);
    }

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->getFullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

Table *Dataset::createTable(const CPLString &name,
                            OGRFeatureDefn * const definition,
                            const Options &options,
                            const Progress &progress)
{
    return static_cast<Table*>(createFeatureClass(name, definition, nullptr,
                                                  wkbNone, options, progress));
}

bool Dataset::destroy()
{
    clear();
    GDALClose(m_DS);
    m_DS = nullptr;
    GDALClose(m_ovrDS);
    m_ovrDS = nullptr;

    if(!File::deleteFile(m_path))
        return false;

    CPLString fullName = getFullName();
    if(m_parent)
        m_parent->notifyChanges();

    Notify::instance().onNotify(fullName, ngsChangeCode::CC_DELETE_OBJECT);

    return true;
}

bool Dataset::isNameValid(const char* name) const
{
    for(const ObjectPtr& object : m_children)
        if(EQUAL(object->getName(), name))
            return false;
    return true;
}

bool forbiddenChar(char c)
{
    return std::find(forbiddenChars.begin(), forbiddenChars.end(), c) !=
            forbiddenChars.end();
}

CPLString Dataset::normalizeDatasetName(const CPLString &name) const
{
    // create unique name
    CPLString outName;
    if(name.empty()) {
        outName = "new_dataset";
    }
    else {
        outName = normalize(name);
        replace_if(outName.begin(), outName.end(), forbiddenChar, '_');
    }

    CPLString originName = outName;
    int nameCounter = 0;
    while(!isNameValid (outName)) {
        outName = originName + "_" + std::to_string(++nameCounter);
        if(nameCounter == MAX_EQUAL_NAMES) {
            return "";
        }
    }

    return outName;
}

CPLString Dataset::normalizeFieldName(const CPLString &name) const
{
    CPLString out = normalize(name); // TODO: add lang support, i.e. ru_RU)

    replace_if( out.begin(), out.end(), forbiddenChar, '_');

    if(isdigit (out[0]))
        out = "Fld_" + out;

    if(Filter::isDatabase(m_type)) {
        // check for sql forbidden names
        if(find(forbiddenSQLFieldNames.begin (), forbiddenSQLFieldNames.end(),
                out) != forbiddenSQLFieldNames.end()) {
            out += "_";
        }
    }

    return out;
}

void Dataset::fillFeatureClasses()
{
    for(int i = 0; i < m_DS->GetLayerCount(); ++i) {
        OGRLayer* layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            const char* layerName = layer->GetName();
            // layer->GetLayerDefn()->GetGeomFieldCount() == 0
            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new Table(layer, this,
                        ngsCatalogObjectType::CAT_TABLE_ANY, layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new FeatureClass(layer, this,
                            ngsCatalogObjectType::CAT_FC_ANY, layerName)));
            }
        }
    }
}

GDALDataset *Dataset::createOverviewDataset()
{
    if(m_ovrDS) {
        return m_ovrDS;
    }

    if(Filter::isDatabase(getType())) {
        m_ovrDS = m_DS;
        m_ovrDS->Reference();
        return m_ovrDS;
    }
    else {
        const char* ovrPath = CPLResetExtension(m_path, OVR_EXT);
        CPLErrorReset();
        GDALDriver *poDriver = Filter::getGDALDriver(
                    ngsCatalogObjectType::CAT_CONTAINER_SQLITE);
        if(poDriver == nullptr) {
            errorMessage(ngsCode::COD_CREATE_FAILED,
                                _("SQLite driver is not present"));
            return nullptr;
        }

        Options options;
        options.addOption("METADATA", "NO");
        options.addOption("SPATIALITE", "NO");
        options.addOption("INIT_WITH_EPSG", "NO");
        auto createOptionsPointer = options.getOptions();

        GDALDataset* DS = poDriver->Create(ovrPath, 0, 0, 0, GDT_Unknown,
                                           createOptionsPointer.get());
        if(DS == nullptr) {
            errorMessage(CPLGetLastErrorMsg());
            return nullptr;
        }
        m_ovrDS = DS;
        return m_ovrDS;
    }
}

const char *Dataset::getOptions(enum ngsOptionType optionType) const
{
    switch (optionType) {
    case OT_CREATE_DATASOURCE:
    case OT_CREATE_LAYER:
    case OT_CREATE_LAYER_FIELD:
    case OT_CREATE_RASTER:
    case OT_OPEN:
        return DatasetBase::getOptions(m_type, optionType);
    case OT_LOAD:
        return "<LoadOptionList>"
               "  <Option name='MOVE' type='boolean' description='If TRUE move dataset, else copy it.' default='FALSE'/>"
               "  <Option name='NEW_NAME' type='string' description='The new name for loaded dataset'/>"
               "  <Option name='ACCEPT_GEOMETRY' type='string-select' description='Load only specific geometry types' default='ANY'>"
               "    <Value>ANY</Value>"
               "    <Value>POINT</Value>"
               "    <Value>LINESTRING</Value>"
               "    <Value>POLYGON</Value>"
               "    <Value>MULTIPOINT</Value>"
               "    <Value>MULTILINESTRING</Value>"
               "    <Value>MULTIPOLYGON</Value>"
               "  </Option>"
               "  <Option name='FORCE_GEOMETRY_TO_MULTI' type='boolean' description='Force input geometry to multi' default='NO'/>"
               "  <Option name='SKIP_EMPTY_GEOMETRY' type='boolean' description='Skip empty geometry' default='NO'/>"
               "  <Option name='SKIP_INVALID_GEOMETRY' type='boolean' description='Skip invalid geometry' default='NO'/>"
               "</LoadOptionList>";
    }
}

bool Dataset::hasChildren()
{
    if(m_childrenLoaded)
        return ObjectContainer::hasChildren();

    if(!isOpened()) {
        if(!open(GDAL_OF_SHARED|GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR)) {
            return false;
        }
    }

    // fill vector layers and tables
    fillFeatureClasses();

    // fill rasters
    char** subdatasetList = m_DS->GetMetadata("SUBDATASETS");
    std::vector<CPLString> siblingFiles;
    if(nullptr != subdatasetList) {
        int i = 0;
        size_t strLen = 0;
        const char* testStr = nullptr;
        char rasterPath[255];
        while((testStr = subdatasetList[i++]) != nullptr) {
            strLen = CPLStrnlen(testStr, 255);
            if(EQUAL(testStr + strLen - 4, "NAME")) {
                CPLStrlcpy(rasterPath, testStr, strLen - 4);
                CPLStringList pathPortions(CSLTokenizeString2( rasterPath, ":", 0 ));
                const char* rasterName = pathPortions[pathPortions.size() - 1];
                m_children.push_back(ObjectPtr(new Raster(siblingFiles, this,
                    ngsCatalogObjectType::CAT_RASTER_ANY, rasterName, rasterPath)));
            }
        }
        CSLDestroy(subdatasetList);
    }

    m_childrenLoaded = true;

    return ObjectContainer::hasChildren();
}

int Dataset::paste(ObjectPtr child, bool move, const Options &options,
                    const Progress &progress)
{
    CPLString newName = options.getStringOption("NEW_NAME",
                                                CPLGetBasename(child->getName()));
    newName = normalizeDatasetName(newName);
    if(move) {
        progress.onProgress(ngsCode::COD_IN_PROCESS, 0.0,
                        _("Move '%s' to '%s'"), newName.c_str (),
                            m_name.c_str ());
    }
    else {
        progress.onProgress(ngsCode::COD_IN_PROCESS, 0.0,
                        _("Copy '%s' to '%s'"), newName.c_str (),
                            m_name.c_str ());
    }

    if(Filter::isTable(child->getType())) {
        TablePtr srcTable = std::dynamic_pointer_cast<Table>(child);
        if(!srcTable) {
            return errorMessage(move ? ngsCode::COD_MOVE_FAILED :
                                       ngsCode::COD_COPY_FAILED,
                                _("Source object '%s' report type TABLE, but it is not a table"),
                                child->getName().c_str());
        }
        OGRFeatureDefn* const srcDefinition = srcTable->getDefinition();
        std::unique_ptr<Table> dstTable(createTable(newName, srcDefinition,
                                                    options));
        if(nullptr == dstTable) {
            return move ? ngsCode::COD_MOVE_FAILED :
                          ngsCode::COD_COPY_FAILED;
        }
        OGRFeatureDefn* const dstDefinition = dstTable->getDefinition();
        // Create fields map. We expected equal count of fields
        FieldMapPtr fieldMap(static_cast<unsigned long>(
                                 dstDefinition->GetFieldCount()));
        for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
            fieldMap[i] = i;
        }

        return dstTable->copyRows(srcTable, fieldMap, progress);

    }
    else if(Filter::isFeatureClass(child->getType())) {
        FeatureClassPtr srcFClass = std::dynamic_pointer_cast<FeatureClass>(child);
        if(!srcFClass) {
            return errorMessage(move ? ngsCode::COD_MOVE_FAILED :
                                       ngsCode::COD_COPY_FAILED,
                                _("Source object '%s' report type FEATURECLASS, but it is not a feature class"),
                                child->getName().c_str());
        }
        bool toMulti = options.getBoolOption("FORCE_GEOMETRY_TO_MULTI", false);
        OGRFeatureDefn* const srcDefinition = srcFClass->getDefinition();
        std::vector<OGRwkbGeometryType> geometryTypes =
                srcFClass->getGeometryTypes();
        OGRwkbGeometryType filterFeometryType =
                FeatureClass::getGeometryTypeFromName(
                    options.getStringOption("ACCEPT_GEOMETRY", "ANY"));
        for(OGRwkbGeometryType geometryType : geometryTypes) {
            if(filterFeometryType != geometryType &&
                    filterFeometryType != wkbUnknown) {
                continue;
            }
            CPLString createName = newName;
            OGRwkbGeometryType newGeometryType = geometryType;
            if(geometryTypes.size () > 1 && filterFeometryType == wkbUnknown) {
                createName += "_";
                createName += FeatureClass::getGeometryTypeName(geometryType,
                                        FeatureClass::GeometryReportType::SIMPLE);
                if(toMulti && geometryType < wkbMultiPoint) {
                    newGeometryType =
                            static_cast<OGRwkbGeometryType>(geometryType + 3);
                }
            }

            std::unique_ptr<FeatureClass> dstFClass(createFeatureClass(createName,
                srcDefinition, srcFClass->getSpatialReference(), geometryType,
                options));
            if(nullptr == dstFClass) {
                return move ? ngsCode::COD_MOVE_FAILED :
                              ngsCode::COD_COPY_FAILED;
            }
            OGRFeatureDefn* const dstDefinition = dstFClass->getDefinition();
            // Create fields map. We expected equal count of fields
            FieldMapPtr fieldMap(static_cast<unsigned long>(
                                               dstDefinition->GetFieldCount()));
            for(int i = 0; i < dstDefinition->GetFieldCount(); ++i) {
                fieldMap[i] = i;
            }

            int result = dstFClass->copyFeatures(srcFClass, fieldMap,
                                                 filterFeometryType,
                                                 progress, options);
            if(result != ngsCode::COD_SUCCESS) {
                return result;
            }
        }
    }
    else {
        // TODO: raster and container support
        return errorMessage(ngsCode::COD_UNSUPPORTED,
                            _("'%s' has unsuported type"), child->getName().c_str());
    }

    if(move) {
        return child->destroy() ? ngsCode::COD_SUCCESS :
                                  ngsCode::COD_DELETE_FAILED;
    }
    return ngsCode::COD_SUCCESS;
}

bool Dataset::canPaste(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly())
        return false;
    return Filter::isFeatureClass(type) || Filter::isTable(type);
}

bool Dataset::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly())
        return false;
    return Filter::isFeatureClass(type) || Filter::isTable(type);
}

TablePtr Dataset::executeSQL(const char* statement, const char* dialect)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened."));
        return TablePtr();
    }
    OGRLayer * layer = m_DS->ExecuteSQL(statement, nullptr, dialect);
    if(nullptr == layer) {
        errorMessage(CPLGetLastErrorMsg());
        return TablePtr();
    }
    if(layer->GetGeomType() == wkbNone)
        return TablePtr(new Table(layer, this,
                                  ngsCatalogObjectType::CAT_QUERY_RESULT));
    else
        return TablePtr(new FeatureClass(layer, this,
                                         ngsCatalogObjectType::CAT_QUERY_RESULT_FC));
}

TablePtr Dataset::executeSQL(const char *statement,
                                    GeometryPtr spatialFilter,
                                    const char *dialect)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened."));
        return TablePtr();
    }
    OGRLayer * layer = m_DS->ExecuteSQL(statement, spatialFilter.get(), dialect);
    if(nullptr == layer) {
        errorMessage(CPLGetLastErrorMsg());
        return TablePtr();
    }
    if(layer->GetGeomType() == wkbNone)
        return TablePtr(new Table(layer, this,
                                  ngsCatalogObjectType::CAT_QUERY_RESULT));
    else
        return TablePtr(new FeatureClass(layer, this,
                                         ngsCatalogObjectType::CAT_QUERY_RESULT_FC));

}

bool Dataset::open(unsigned int openFlags, const Options &options)
{
    bool result = DatasetBase::open(m_path, openFlags, options);
    if(result) {
        if(Filter::isDatabase(getType())) {
            m_ovrDS = m_DS;
            m_ovrDS->Reference();
        }
        else {
            const char* ovrPath = CPLResetExtension(m_path, OVR_EXT);
            if(Folder::isExists(ovrPath)) {
                m_ovrDS = static_cast<GDALDataset*>(GDALOpenEx(ovrPath, openFlags,
                                                               nullptr, nullptr,
                                                               nullptr));

                if(m_ovrDS == nullptr) {
                    warningMessage(CPLGetLastErrorMsg());
                }
            }
        }
    }
    return result;
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

//DatasetPtr Dataset::create(const char* path, const char* driver,
//                           char **options)
//{
//    DatasetPtr out;
//    if(nullptr == path || EQUAL(path, "")) {
//        CPLError(CE_Failure, CPLE_AppDefined, _("The path is empty"));
//        return out;
//    }

//    // get GeoPackage driver
//    GDALDriver *poDriver = GetGDALDriverManager()->GetDriverByName(driver);
//    if( poDriver == nullptr ) {
//        CPLError(CE_Failure, CPLE_AppDefined, _("The GDAL driver '%s' not found"),
//                 driver);
//        return out;
//    }

//    // create
//    CPLErrorReset();
//    GDALDatasetPtr DS = poDriver->Create( path, 0, 0, 0, GDT_Unknown, options );
//    if( DS == nullptr ) {
//        return out;
//    }

//    return getDatasetForGDAL(path, DS);
//}

//DatasetPtr Dataset::getDatasetForGDAL(const CPLString& path, GDALDatasetPtr ds)
//{
//    GDALDriver* driver = ds->GetDriver ();

//    DatasetPtr out;
//    Dataset* outDS = nullptr;
//    if((testBoolean(driver->GetMetadataItem (GDAL_DMD_SUBDATASETS), false) &&
//       !testBoolean(driver->GetMetadataItem (GDAL_DCAP_RASTER), false) ) ||
//            ds->GetLayerCount () > 1) {
//        outDS = new DatasetContainer();
//    }
//    else if (testBoolean(driver->GetMetadataItem (GDAL_DCAP_RASTER), false)) {
//        RasterDataset* raster = new RasterDataset();
//        raster->m_spatialReference.SetFromUserInput (ds->GetProjectionRef ());
//        outDS = raster;
//    }
//    else { //if (testBoolean(driver->GetMetadataItem (GDAL_DCAP_VECTOR), false))
//        OGRLayer* layer = ds->GetLayer (0);
//        if(nullptr != layer){
//            OGRFeatureDefn* defn = layer->GetLayerDefn ();
//            if( defn->GetGeomFieldCount () == 0 ) {
//                outDS = new Table(layer);
//            }
//            else {
//                outDS = new FeatureDataset(layer);
//            }
//        }
//    }

//    if (!outDS) {
//        return nullptr;
//    }

//    outDS->m_DS = ds;
//    outDS->m_path = path;
//    outDS->m_opened = true;

//    out.reset (outDS);
//    return out;
//}

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

//DatasetContainer::DatasetContainer(ObjectContainer * const parent,
//                                   const ngsCatalogObjectType type,
//                                   const CPLString &name,
//                                   const CPLString &path) :
//    ObjectContainer(parent, type, name, path), m_DS(nullptr),
//    m_hLoadThread(nullptr), m_cancelLoad(false), m_newTaskId(0)
//{
//}

//DatasetContainer::~DatasetContainer()
//{
//    if(nullptr != m_hLoadThread) {
//        m_cancelLoad = true;
//        // wait thread exit
//        CPLJoinThread(m_hLoadThread);
//    }
//}


} // namespace ngs


