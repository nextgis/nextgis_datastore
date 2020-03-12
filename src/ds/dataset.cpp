/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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

#include "dataset.h"

// stl
#include <algorithm>
#include <array>
#include <stdio.h>

#include "featureclass.h"
#include "raster.h"
#include "simpledataset.h"
#include "table.h"
#include "catalog/file.h"
#include "catalog/folder.h"
#include "ngstore/api.h"
#include "ngstore/catalog/filter.h"
#include "ngstore/version.h"

#include "util.h"
#include "util/account.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/stringutil.h"

namespace ngs {

constexpr std::array<char, 23> forbiddenChars = {{':', '@', '#', '%', '^', '&',
    '*','!', '$', '(', ')', '+', '-', '?', '=', '/', '\\', '"', '\'', '[', ']',
    ',', ' '}};

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

//------------------------------------------------------------------------------
// GDALDatasetPtr
//------------------------------------------------------------------------------

GDALDatasetPtr::GDALDatasetPtr(GDALDataset *DS) : shared_ptr(DS, GDALClose)
{
}

GDALDatasetPtr::GDALDatasetPtr() : shared_ptr(nullptr, GDALClose)
{

}

GDALDatasetPtr::GDALDatasetPtr(const GDALDatasetPtr &DS) : shared_ptr(DS)
{
}

GDALDatasetPtr &GDALDatasetPtr::operator=(GDALDataset *DS)
{
    reset(DS);
    return *this;
}

GDALDatasetPtr::operator GDALDataset *() const
{
    return get();
}

//------------------------------------------------------------------------------
// DatasetBase
//------------------------------------------------------------------------------
DatasetBase::DatasetBase()
{

}

DatasetBase::~DatasetBase()
{
}

void DatasetBase::close()
{
    m_DS = nullptr;
}

bool DatasetBase::startTransaction(bool force)
{
    if(!isOpened()) {
        return false;
    }
    return m_DS->StartTransaction(force ? TRUE : FALSE) == OGRERR_NONE;
}

bool DatasetBase::commitTransaction()
{
    if(!isOpened()) {
        return false;
    }
    return m_DS->CommitTransaction() == OGRERR_NONE;
}

bool DatasetBase::rollbackTransaction()
{
    if(!isOpened()) {
        return false;
    }
    return m_DS->RollbackTransaction() == OGRERR_NONE;
}

void DatasetBase::flushCache()
{
    if(!isOpened()) {
        return;
    }
    m_DS->FlushCache();
}

bool DatasetBase::isOpened() const
{
     return m_DS != nullptr;
}

std::string DatasetBase::options(const enum ngsCatalogObjectType type,
                                    ngsOptionType optionType) const
{
    GDALDriver *driver = Filter::getGDALDriver(type);
    switch (optionType) {
    case OT_CREATE_DATASOURCE:
        if(nullptr == driver)
            return "";
        return fromCString(driver->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST));
    case OT_CREATE_LAYER:
        if(nullptr == driver)
            return "";
        return fromCString(driver->GetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST));
    case OT_CREATE_LAYER_FIELD:
        if(nullptr == driver)
            return "";
        return fromCString(driver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES));
    case OT_CREATE_RASTER:
        if(nullptr == driver)
            return "";
        return fromCString(driver->GetMetadataItem(GDAL_DMD_CREATIONDATATYPES));
    case OT_OPEN:
        if(nullptr == driver)
            return "";
        return fromCString(driver->GetMetadataItem(GDAL_DMD_OPENOPTIONLIST));
    case OT_LOAD:
        return "";
    }
    return "";
}

bool DatasetBase::open(const std::string &path, unsigned int openFlags,
                       const Options &options)
{
    if(path.empty()) {
        return errorMessage(_("The path is empty"));
    }

    // NOTE: VALIDATE_OPEN_OPTIONS can be set to NO to avoid warnings

    resetError();
    auto openOptions = options.asCPLStringList();
    m_DS = static_cast<GDALDataset*>(GDALOpenEx(path.c_str(), openFlags, nullptr,
                                                openOptions, nullptr));
    if(nullptr == m_DS) {
        errorMessage(_("Failed to open dataset %s. %s"), path.c_str(), CPLGetLastErrorMsg());
        if(openFlags & GDAL_OF_UPDATE) {
            // Try to open read-only
            openFlags &= static_cast<unsigned int>(~GDAL_OF_UPDATE);
            openFlags |= GDAL_OF_READONLY;
            m_DS = static_cast<GDALDataset*>(GDALOpenEx( path.c_str(), openFlags,
                                                         nullptr, openOptions,
                                                         nullptr));
            if(nullptr == m_DS) {
                errorMessage(_("Failed to open dataset %s. %s"), path.c_str(), CPLGetLastErrorMsg());
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
constexpr const char *ADDS_EXT = "ngadds";

// Metadata
constexpr const char *META_KEY = "key";
constexpr unsigned short META_KEY_LIMIT = 128;
constexpr const char *META_VALUE = "value";
constexpr unsigned short META_VALUE_LIMIT = 512;

// Attachments
constexpr const char *ATTACH_SUFFIX = "attachments";

// History
constexpr const char *HISTORY_SUFFIX = "editlog";

Dataset::Dataset(ObjectContainer * const parent,
                 const enum ngsCatalogObjectType type,
                 const std::string &name,
                 const std::string &path) :
    ObjectContainer(parent, type, name, path),
    DatasetBase(),
    m_metadata(nullptr)
{
}

FeatureClass *Dataset::createFeatureClass(const std::string &name,
                                          enum ngsCatalogObjectType objectType,
                                          OGRFeatureDefn * const definition,
                                          SpatialReferencePtr spatialRef,
                                          OGRwkbGeometryType type,
                                          const Options& options,
                                          const Progress& progress)
{
    if(!isOpened()) {
        errorMessage(_("Not opened"));
        return nullptr;
    }

    resetError();
    OGRLayer *layer = m_DS->CreateLayer(name.c_str(), spatialRef, type,
                                        options.asCPLStringList());

    if(layer == nullptr) {
        errorMessage(_("Failed to create layer %s. %s"), name.c_str(), CPLGetLastErrorMsg());
        return nullptr;
    }

    std::vector<std::string> nameList;
    for (int i = 0; i < definition->GetFieldCount(); ++i) {
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        std::string newFieldName = normalizeFieldName(srcField->GetNameRef(), nameList);
        if(!compare(newFieldName, srcField->GetNameRef())) {
            progress.onProgress(COD_WARNING, 0.0,
                                _("Field %s of source table was renamed to %s in destination table"),
                                srcField->GetNameRef(), newFieldName.c_str());
        }

        dstField.SetName(newFieldName.c_str());
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(_("Failed to create field %s. %s"), newFieldName.c_str(),
                         CPLGetLastErrorMsg());
            return nullptr;
        }
        nameList.push_back(newFieldName);
    }

    return new FeatureClass(layer, this, objectType, name);
}

Table *Dataset::createTable(const std::string &name,
                            enum ngsCatalogObjectType objectType,
                            OGRFeatureDefn * const definition,
                            const Options &options,
                            const Progress &progress)
{
    return static_cast<Table*>(
                createFeatureClass(name, objectType, definition, nullptr, wkbNone,
                                   options, progress));
}

bool Dataset::setProperty(const std::string &key, const std::string &value,
                          const std::string &domain)
{
    if(!open(DatasetBase::defaultOpenFlags)) {
        return false;
    }

    if(!isOpened()) {
        return false;
    }

    createAdditionsDataset();

    if(nullptr == m_metadata) {
        m_metadata = createMetadataTable(m_addsDS);
        if(nullptr == m_metadata) {
            return false;
        }
    }

    // Update or set property
    auto keyStr = domain + "." + key;
    MutexHolder holder(m_executeSQLMutex);

    m_metadata->SetAttributeFilter(CPLSPrintf("'%s' = \"%s\"", META_KEY, keyStr.c_str()));
    FeaturePtr feature = m_metadata->GetNextFeature();
    m_metadata->SetAttributeFilter(nullptr);
    if(feature) {
        feature->SetField(META_VALUE, value.c_str());
        return m_metadata->SetFeature(feature) == OGRERR_NONE;
    }

    feature = OGRFeature::CreateFeature(m_metadata->GetLayerDefn());
    feature->SetField(META_KEY, keyStr.c_str());
    feature->SetField(META_VALUE, value.c_str());
    return m_metadata->CreateFeature(feature) == OGRERR_NONE;
}

std::string Dataset::property(const std::string &key,
                              const std::string &defaultValue,
                              const std::string &domain) const
{
    if(!m_DS) {
        return ObjectContainer::property(key, defaultValue, domain);
    }

    auto out = fromCString(m_DS->GetMetadataItem(key.c_str(), domain.c_str()));
    if(!out.empty()) {
        return out;
    }

    if(nullptr == m_metadata) {
        return ObjectContainer::property(key, defaultValue, domain);
    }

    auto keyStr = domain + "." + key;
    MutexHolder holder(m_executeSQLMutex);

    m_metadata->SetAttributeFilter(CPLSPrintf("%s LIKE \"%s\"", META_KEY, keyStr.c_str()));
    FeaturePtr feature = m_metadata->GetNextFeature();
    m_metadata->SetAttributeFilter(nullptr);
    if(feature) {
        out = feature->GetFieldAsString(1);
    }

    if(!out.empty()) {
        return out;
    }

    return ObjectContainer::property(key, defaultValue, domain);
}

void Dataset::lockExecuteSql(bool lock)
{
    if(lock) {
        m_executeSQLMutex.acquire(15.0);
    }
    else {
        m_executeSQLMutex.release();
    }
}

bool Dataset::destroyTable(Table *table)
{
    if(destroyTable(m_DS, table->m_layer)) {
        deleteProperties(table->name());
        destroyAttachmentsTable(table->name()); // Attachments table maybe not exists
        destroyEditHistoryTable(table->name()); // Log edit table may be not exists
        return true;
    }
    return false;
}

bool Dataset::destroy()
{
    clear();
    m_DS = nullptr;
    m_addsDS = nullptr;

    if(Filter::isLocalDir(m_type)) {
        if(!Folder::rmDir(m_path)) {
            return false;
        }
    }
    else {
        if(!File::deleteFile(m_path)) {
            return false;
        }
    }

    // Delete additions
    if(!Filter::isDatabase(type())) {
        std::string additionsDSPath = additionsDatasetPath();
        if(Folder::isExists(additionsDSPath)) {
            File::deleteFile(additionsDSPath);
        }
    }
    std::string attachmentsPath = attachmentsFolderPath();
    if(Folder::isExists(attachmentsPath)) {
        Folder::rmDir(attachmentsPath);
    }

    // aux.xml
    std::string auxPath = m_path + ".aux.xml";
    if(Folder::isExists(auxPath)) {
        if(File::deleteFile(auxPath)) {
            return ObjectContainer::destroy();
        }
        else {
            return false;
        }
    }

    return true;
}

bool Dataset::canDestroy() const
{
    return !Folder::isReadOnly(m_path);
}

Properties Dataset::properties(const std::string &domain) const {
    if(nullptr == m_DS) {
        return ObjectContainer::properties(domain);
    }

    MutexHolder holder(m_executeSQLMutex);

    // 1. Get gdal metadata
    Properties out(m_DS->GetMetadata(domain.c_str()));
    out.append(ObjectContainer::properties(domain));

    if(nullptr == m_metadata) {
        return out;
    }

    // 2. Get ngs properties
    m_metadata->SetAttributeFilter(CPLSPrintf("%s LIKE \"%s.%%\"", META_KEY,
                                              domain.c_str()));
    FeaturePtr feature;
    while((feature = m_metadata->GetNextFeature())) {
        std::string key = feature->GetFieldAsString(0) + domain.size() + 1;
        std::string value = feature->GetFieldAsString(1);

        out.add(key, value);
    }
    m_metadata->SetAttributeFilter(nullptr);

    return out;
}

void Dataset::deleteProperties(const std::string &domain)
{
    m_DS->SetMetadata(nullptr, domain.c_str());

    if(nullptr == m_metadata) {
        return;
    }
    executeSQL(CPLSPrintf("DELETE FROM %s WHERE %s LIKE \"%s.%%\"",
                          METADATA_TABLE_NAME, META_KEY, domain.c_str()));
}

bool Dataset::isNameValid(const std::string &name) const
{
    if(name.empty()) {
        return false;
    }

    for(const ObjectPtr &object : m_children) {
        if(compare(object->name(), name)) {
            return false;
        }
    }

    if(compare(METADATA_TABLE_NAME, name)) {
        return false;
    }
    return true;
}

bool Dataset::forbiddenChar(char c)
{
    return std::find(forbiddenChars.begin(), forbiddenChars.end(), c) !=
            forbiddenChars.end();
}

GDALDataset *Dataset::createAdditionsDatasetInt(const std::string &path,
                                                enum ngsCatalogObjectType type)
{
    resetError();
    GDALDriver *driver = Filter::getGDALDriver(type);
    if(driver == nullptr) {
        outMessage(COD_CREATE_FAILED, _("Driver is not present"));
        return nullptr;
    }

    Options options;
    options.add("METADATA", "NO");
    options.add("SPATIALITE", "NO");
    options.add("INIT_WITH_EPSG", "NO");

    GDALDataset *DS = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown,
                                       options.asCPLStringList());
    if(DS == nullptr) {
        errorMessage(_("Failed to create additional dataset on path %s. %s"),
                     path.c_str(), CPLGetLastErrorMsg());
    }
    return DS;
}

std::string Dataset::normalizeDatasetName(const std::string &name) const
{
    // create unique name
    std::string outName;
    if(name.empty()) {
        outName = "new_dataset";
    }
    else {
        outName = normalize(name);
        replace_if(outName.begin(), outName.end(), forbiddenChar, '_');
    }

    std::string originName = outName;
    int nameCounter = 0;
    while(!isNameValid (outName)) {
        outName = originName + "_" + std::to_string(++nameCounter);
        if(nameCounter == MAX_EQUAL_NAMES) {
            return "";
        }
    }

    return outName;
}

std::string Dataset::normalizeFieldName(const std::string &name,
                                        const std::vector<std::string> &nameList,
                                        int counter) const
{
    std::string out;
    std::string processedName;
    if(counter == 0) {
        out = normalize(name, "ru"); // TODO: add lang support, i.e. ru_RU)

        replace_if(out.begin(), out.end(), forbiddenChar, '_');

        if(isdigit(out[0])) {
            out = "fld_" + out;
        }

        if(Filter::isDatabase(m_type)) {
            // Check for sql forbidden names
            std::string testFb = CPLString(out).toupper();
            if(find(forbiddenSQLFieldNames.begin(), forbiddenSQLFieldNames.end(),
                    testFb) != forbiddenSQLFieldNames.end()) {
                out += "_";
            }
        }
        processedName = out;
    }
    else {
        out = name + "_" + std::to_string(counter);
        processedName = name;
    }

    auto it = std::find(nameList.begin(), nameList.end(), out);
    if(it == nameList.end()) {
        return out;
    }
    return normalizeFieldName(processedName, nameList, counter++);
}

bool Dataset::skipFillFeatureClass(OGRLayer *layer) const
{
    if(compare(layer->GetName(), METADATA_TABLE_NAME)) {
        return true;
    }
    return comparePart(layer->GetName(), NG_PREFIX, NG_PREFIX_LEN);
}

void Dataset::fillFeatureClasses() const
{
    if(nullptr == m_DS) {
        return;
    }

    for(int i = 0; i < m_DS->GetLayerCount(); ++i) {
        OGRLayer *layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            // Hide metadata, overviews, history tables
            if(skipFillFeatureClass(layer)) {
                continue;
            }

            const char *layerName = layer->GetName();
            Dataset *parent = const_cast<Dataset*>(this);
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new Table(layer, parent,
                        CAT_TABLE_ANY, layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new FeatureClass(layer, parent,
                            CAT_FC_ANY, layerName)));
            }
        }
    }
}

GDALDatasetPtr Dataset::createAdditionsDataset()
{
    if(m_addsDS) {
        return m_addsDS;
    }

    if(Filter::isDatabase(type())) {
        m_addsDS = m_DS;
    }
    else {
        m_addsDS = createAdditionsDatasetInt(additionsDatasetPath(),
                                             CAT_CONTAINER_SQLITE);
    }
    return m_addsDS;
}

std::string Dataset::additionsDatasetPath() const
{
    return File::resetExtension(m_path, additionsDatasetExtension());
}

std::string Dataset::attachmentsFolderPath(bool create) const
{
    auto attachmentsPath = File::resetExtension(m_path, ATTACH_SUFFIX);
    if(create && !Folder::isExists(attachmentsPath)) {
        Folder::mkDir(attachmentsPath, true);
    }
    return attachmentsPath;
}

std::string Dataset::options(enum ngsOptionType optionType) const
{
    switch (optionType) {
    case OT_CREATE_DATASOURCE:
    case OT_CREATE_LAYER:
    case OT_CREATE_LAYER_FIELD:
    case OT_CREATE_RASTER:
    case OT_OPEN:
        return DatasetBase::options(m_type, optionType);
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
               "  <Option name='CREATE_OVERVIEWS_TABLE' type='boolean' description='Create empty overviews table' default='NO'/>"
               "  <Option name='CREATE_OVERVIEWS' type='boolean' description='Create overviews table and fill it with overviews. The level should be set by ZOOM_LEVELS option' default='NO'/>"
               "  <Option name='ZOOM_LEVELS' type='string' description='Comma separated list of zoom level' default=''/>"
               "</LoadOptionList>";
    }

    return "";
}

bool DatasetBase::isReadOnly(GDALDataset *DS)
{
    if(DS == nullptr) {
        return true;
    }

    // https://github.com/OSGeo/gdal/issues/2162
#if GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(3,1,0)
    return DS->GetAccess() == GA_ReadOnly;
#else
    return false;
#endif
}

bool Dataset::isReadOnly() const
{
    return DatasetBase::isReadOnly(m_DS);
}

/**
 * @brief Dataset::paste Copy or move feature class or table to destination catalog object.
 * @param child Feature class or table to copy or move.
 * @param move Copy or move.
 * @param options Key - value list. The available values are:
 * - NEW_NAME - new table/featureClass name. If not present, the original Object name will use.
 * - CREATE_UNIQUE - if name of new object already present, the counter suffix will append.
 * - OGR_STYLE_FIELD_TO_STRING - copy OGR Style to field named 'OGR_STYLE'.
 * - FORCE_GEOMETRY_TO_MULTI - if feature class has mixed geometry types (i.e. polygons and multypolygons) the non multi types will force to multi.
 * - SKIP_EMPTY_GEOMETRY - if geometry is empty, skip to add feature to destination table/featureClass.
 * - SKIP_INVALID_GEOMETRY - if true, check validity of source geometry. Features with invalid geometries will skip.
 * - DESCRIPTION - If supported by Object the description will add.
 * - ACCEPT_GEOMETRY - limit accepted geometry type. Defaults to ALL: no limits.
 * @param progress
 * @return
 */
int Dataset::paste(ObjectPtr child, bool move, const Options &options,
                   const Progress &progress)
{
    loadChildren();

    std::string newName = options.asString("NEW_NAME",
                                           File::getBaseName(child->name()));
    newName = normalizeDatasetName(newName);
    if(newName.empty()) {
        errorMessage(_("Failed to create unique name."));
        return COD_LOAD_FAILED;
    }
    if(move) {
        progress.onProgress(COD_IN_PROCESS, 0.0,
                        _("Move '%s' to '%s'"), newName.c_str(), m_name.c_str());
    }
    else {
        progress.onProgress(COD_IN_PROCESS, 0.0,
                        _("Copy '%s' to '%s'"), newName.c_str(), m_name.c_str ());
    }

    if(child->type() == CAT_CONTAINER_SIMPLE) {
        auto dataset = ngsDynamicCast(SingleLayerDataset, child);
        child = dataset->internalObject();
    }

    if(!child) {
        return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                          _("Source object is invalid"));
    }

    if(Filter::isTable(child->type())) {
        TablePtr srcTable = std::dynamic_pointer_cast<Table>(child);
        if(!srcTable) {
            return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                                _("Source object '%s' report type TABLE, but it is not a table"),
                                child->name().c_str());
        }

        if(srcTable->featureCount() > MAX_FEATURES4UNSUPPORTED) {
            const char *appName = CPLGetConfigOption("APP_NAME", "ngstore");
            if(!Account::instance().isFunctionAvailable(appName, "paste_features")) {
                return outMessage(COD_FUNCTION_NOT_AVAILABLE,
                                  _("Cannot %s " CPL_FRMT_GIB " features on your plan, or account is not authorized"),
                                  move ? _("move") : _("copy"), srcTable->featureCount());
            }
        }

        auto srcDefinition = srcTable->definition();
        auto dstTable = createTable(newName, CAT_TABLE_ANY, srcDefinition, options);
        if(nullptr == dstTable) {
            return move ? COD_MOVE_FAILED : COD_COPY_FAILED;
        }

        Progress progressMulti(progress);
        progressMulti.setTotalSteps(2);
        progressMulti.setStep(0);

        // Create fields map. We expected equal count of fields
        FieldMapPtr fieldMap(srcTable->fields(), dstTable->fields());
        int result = dstTable->copyRows(srcTable, fieldMap, progressMulti);
        if(result != COD_SUCCESS) {
            delete dstTable;
            return result;
        }

        auto fullNameStr = dstTable->fullName();
        progressMulti.setStep(1);
        // Execute postprocess after features copied.
        if(!dstTable->onRowsCopied(srcTable, progressMulti, options)) {
            warningMessage(_("Postprocess features after copy in feature class '%s' failed."),
                           fullNameStr.c_str());
        }
        progressMulti.onProgress(COD_FINISHED, 1.0, "");

        onChildCreated(dstTable);
    }
    else if(Filter::isFeatureClass(child->type())) {
        FeatureClassPtr srcFClass = std::dynamic_pointer_cast<FeatureClass>(child);
        if(!srcFClass) {
            return outMessage(move ? COD_MOVE_FAILED : COD_COPY_FAILED,
                _("Source object '%s' report type FEATURECLASS, but it is not a feature class"),
                child->name().c_str());
        }

        if(srcFClass->featureCount() > MAX_FEATURES4UNSUPPORTED) {
            const char *appName = CPLGetConfigOption("APP_NAME", "ngstore");
            if(!Account::instance().isFunctionAvailable(appName, "paste_features")) {
                return outMessage(COD_FUNCTION_NOT_AVAILABLE,
                    _("Cannot %s " CPL_FRMT_GIB " features on your plan, or account is not authorized"),
                    move ? _("move") : _("copy"), srcFClass->featureCount());
            }
        }

        bool toMulti = options.asBool("FORCE_GEOMETRY_TO_MULTI", false);
        auto srcDefinition = srcFClass->definition();

        bool ogrStyleFieldToStyle = options.asBool("OGR_STYLE_FIELD_TO_STRING", false);
        if(ogrStyleFieldToStyle) {
            srcDefinition->DeleteFieldDefn(srcDefinition->GetFieldIndex(OGR_STYLE_FIELD));
        }

        std::vector<OGRwkbGeometryType> geometryTypes =
                srcFClass->geometryTypes();
        OGRwkbGeometryType filterGeometryType =
                FeatureClass::geometryTypeFromName(
                    options.asString("ACCEPT_GEOMETRY", "ANY"));
        for(OGRwkbGeometryType geometryType : geometryTypes) {
            if(filterGeometryType != geometryType &&
                    filterGeometryType != wkbUnknown) {
                continue;
            }
            std::string createName = newName;
            OGRwkbGeometryType newGeometryType = geometryType;
            if(geometryTypes.size () > 1 && filterGeometryType == wkbUnknown) {
                createName += "_";
                createName += FeatureClass::geometryTypeName(geometryType,
                                        FeatureClass::GeometryReportType::SIMPLE);
                if(toMulti && OGR_GT_Flatten(geometryType) < wkbMultiPoint) {
                    newGeometryType =
                            static_cast<OGRwkbGeometryType>(geometryType + 3);
                }
            }

            auto dstFClass = createFeatureClass(createName,
                CAT_FC_ANY, srcDefinition, srcFClass->spatialReference(),
                newGeometryType, options);
            if(nullptr == dstFClass) {
                return move ? COD_MOVE_FAILED : COD_COPY_FAILED;
            }
            auto dstFields = dstFClass->fields();
            // Create fields map. We expected equal count of fields
            FieldMapPtr fieldMap(srcFClass->fields(), dstFClass->fields());

            Progress progressMulti(progress);
            progressMulti.setTotalSteps(2);
            progressMulti.setStep(0);

            int result = dstFClass->copyFeatures(srcFClass, fieldMap,
                                                 filterGeometryType,
                                                 progressMulti, options);
            if(result != COD_SUCCESS) {
                delete dstFClass;
                return result;
            }
            auto fullNameStr = dstFClass->fullName();

            progressMulti.setStep(1);
            // Execute postprocess after features copied.
            if(!dstFClass->onRowsCopied(srcFClass, progressMulti, options)) {
                warningMessage(_("Postprocess features after copy in feature class '%s' failed."),
                               fullNameStr.c_str());
            }
            progressMulti.onProgress(COD_FINISHED, 1.0, "");

            onChildCreated(dstFClass);
        }
    }
    else {
        // TODO: raster and container support
        return outMessage(COD_UNSUPPORTED,
                          _("'%s' has unsuported type"), child->name().c_str());
    }

    if(move) {
        return child->destroy() ? COD_SUCCESS : COD_DELETE_FAILED;
    }
    return COD_SUCCESS;
}

bool Dataset::canPaste(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly()) {
        return false;
    }
    return Filter::isFeatureClass(type) || Filter::isTable(type);
}

bool Dataset::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly()) {
        return false;
    }
    return Filter::isFeatureClass(type) || Filter::isTable(type);
}

std::string Dataset::additionsDatasetExtension()
{
    return ADDS_EXT;
}

std::string Dataset::attachmentsFolderExtension()
{
    return ATTACH_SUFFIX;
}

Dataset *Dataset::create(ObjectContainer * const parent,
                        const enum ngsCatalogObjectType type,
                        const std::string &name,
                        const Options &options)
{
    GDALDriver *driver = Filter::getGDALDriver(type);
    if(nullptr == driver) {
        return nullptr;
    }

    Dataset *out;
    std::string path = File::formFileName(parent->path(), name,
                                          Filter::extension(type));
    if(options.asBool("OVERWRITE")) {
        auto ovr = parent->getChild(File::formFileName("", name,
                                                       Filter::extension(type)));
        if(ovr && !ovr->destroy()) {
            return nullptr;
        }
    }

    if(Filter::isSimpleDataset(type)) {
        out = new Dataset(parent, CAT_CONTAINER_SIMPLE, name, path);
    }
    else {
        out = new Dataset(parent, type, name, path);
    }

    out->m_DS = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown,
                               options.asCPLStringList());

    return out;
}

TablePtr Dataset::executeSQL(const std::string &statement, const std::string &dialect)
{
    return executeSQL(statement, GeometryPtr(), dialect);
}

TablePtr Dataset::executeSQL(const std::string &statement,
                             GeometryPtr spatialFilter,
                             const std::string &dialect)
{
    if(!isOpened()) {
        return TablePtr();
    }

    OGRGeometry *spaFilter(nullptr);

    if(spatialFilter) {
        spaFilter = spatialFilter.get();
    }

    MutexHolder holder(m_executeSQLMutex);
    resetError();

    OGRLayer *layer = m_DS->ExecuteSQL(statement.c_str(), spaFilter, dialect.c_str());
    if(nullptr == layer) {
        errorMessage(_("Execute SQL failed. Empty result. %s"), CPLGetLastErrorMsg());
        return TablePtr();
    }
    if(layer->GetGeomType() == wkbNone) {
        return TablePtr(new Table(layer, this, CAT_QUERY_RESULT));
    }
    else {
        return TablePtr(new FeatureClass(layer, this, CAT_QUERY_RESULT_FC));
    }
}

bool Dataset::open(unsigned int openFlags, const Options &options)
{
    if(isOpened()) {
        return true;
    }

    bool result = DatasetBase::open(m_path, openFlags, options);
    if(result) {
        if(Filter::isDatabase(type())) {
            m_addsDS = m_DS;
        }
        else {
            std::string addsPath = additionsDatasetPath();
            if(Folder::isExists(addsPath)) {
                m_addsDS = static_cast<GDALDataset*>(
                    GDALOpenEx(addsPath.c_str(), openFlags, nullptr, nullptr, nullptr));
            }
        }

        if(m_addsDS == nullptr) {
            warningMessage(CPLGetLastErrorMsg());
        }
        else {
            m_metadata = m_addsDS->GetLayerByName(METADATA_TABLE_NAME);
        }
    }
    return result;
}

OGRLayer *Dataset::createMetadataTable(GDALDataset *ds)
{
    resetError();
    if(nullptr == ds) {
        return nullptr;
    }

    OGRLayer *metadataLayer = ds->CreateLayer(METADATA_TABLE_NAME, nullptr,
                                                   wkbNone, nullptr);
    if (nullptr == metadataLayer) {
        return nullptr;
    }

    OGRFieldDefn fieldKey(META_KEY, OFTString);
    fieldKey.SetWidth(META_KEY_LIMIT);
    OGRFieldDefn fieldValue(META_VALUE, OFTString);
    fieldValue.SetWidth(META_VALUE_LIMIT);

    if(metadataLayer->CreateField(&fieldKey) != OGRERR_NONE ||
       metadataLayer->CreateField(&fieldValue) != OGRERR_NONE) {
        return nullptr;
    }

    FeaturePtr feature(OGRFeature::CreateFeature(metadataLayer->GetLayerDefn()));

    // write version
    auto keyStr = std::string(NG_ADDITIONS_KEY) + "." + NGS_VERSION_KEY;
    feature->SetField(META_KEY, keyStr.c_str());
    feature->SetField(META_VALUE, NGS_VERSION_NUM);
    if(metadataLayer->CreateFeature(feature) != OGRERR_NONE) {
        outMessage(COD_WARNING, _("Failed to add version to methadata"));
    }

    if(ds->GetDriver() == Filter::getGDALDriver(CAT_CONTAINER_GPKG)) {
        ds->SetMetadataItem(NGS_VERSION_KEY, CPLSPrintf("%d", NGS_VERSION_NUM),
                            NG_ADDITIONS_KEY);
    }

    return metadataLayer;
}

bool Dataset::destroyTable(GDALDataset *ds, OGRLayer *layer)
{
    for(int i = 0; i < ds->GetLayerCount(); ++i) {
        if(ds->GetLayer(i) == layer) {
            resetError();
            return ds->DeleteLayer(i) == OGRERR_NONE;
        }
    }
    return true;
}

OGRLayer *Dataset::createEditHistoryTable(GDALDataset *ds, const std::string &name)
{
    OGRLayer *logLayer = ds->CreateLayer(name.c_str(), nullptr, wkbNone, nullptr);
    if (nullptr == logLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create table  fields
    OGRFieldDefn fidField(FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn afidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn opField(OPERATION_FIELD, OFTInteger64);
    OGRFieldDefn metaField(META_FIELD, OFTString);
    metaField.SetWidth(64);

    if(logLayer->CreateField(&fidField) != OGRERR_NONE ||
       logLayer->CreateField(&afidField) != OGRERR_NONE ||
       logLayer->CreateField(&opField) != OGRERR_NONE ||
       logLayer->CreateField(&metaField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return logLayer;
}

OGRLayer *Dataset::createAttachmentsTable(GDALDataset *ds, const std::string &name)
{
    OGRLayer *attLayer = ds->CreateLayer(name.c_str(), nullptr, wkbNone, nullptr);
    if (nullptr == attLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Create table  fields
    OGRFieldDefn fidField(ATTACH_FEATURE_ID_FIELD, OFTInteger64);
    OGRFieldDefn nameField(ATTACH_FILE_NAME_FIELD, OFTString);
    OGRFieldDefn descField(ATTACH_DESCRIPTION_FIELD, OFTString);
    OGRFieldDefn dataField(ATTACH_DATA_FIELD, OFTBinary);

    // TODO: Add data field

    if(attLayer->CreateField(&fidField) != OGRERR_NONE ||
       attLayer->CreateField(&nameField) != OGRERR_NONE ||
       attLayer->CreateField(&descField) != OGRERR_NONE ||
       attLayer->CreateField(&dataField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return attLayer;
}

OGRLayer *Dataset::createAttachmentsTable(const std::string &name)
{
    createAdditionsDataset();

    if(!m_addsDS) {
        return nullptr;
    }

    return createAttachmentsTable(m_addsDS, attachmentsTableName(name));
}

bool Dataset::destroyAttachmentsTable(const std::string &name)
{
    OGRLayer *layer = getAttachmentsTable(name);
    if(!layer) {
        return false;
    }
    return destroyTable(m_DS, layer);
}

OGRLayer *Dataset::getAttachmentsTable(const std::string &name)
{
    if(!m_addsDS)
        return nullptr;

    return m_addsDS->GetLayerByName(attachmentsTableName(name).c_str());
}

OGRLayer *Dataset::createEditHistoryTable(const std::string &name)
{
    createAdditionsDataset();

    if(!m_addsDS) {
        return nullptr;
    }

    return createEditHistoryTable(m_addsDS, historyTableName(name));
}

bool Dataset::destroyEditHistoryTable(const std::string &name)
{
    OGRLayer *layer = getEditHistoryTable(name);
    if(!layer) {
        return false;
    }
    return destroyTable(m_DS, layer);
}

OGRLayer *Dataset::getEditHistoryTable(const std::string &name)
{
    if(!m_addsDS)
        return nullptr;
    return m_addsDS->GetLayerByName(historyTableName(name).c_str());
}

void Dataset::clearEditHistoryTable(const std::string &name)
{
    deleteFeatures(historyTableName(name));
}

std::string Dataset::historyTableName(const std::string &name) const
{
    return NG_PREFIX + name + "_" + HISTORY_SUFFIX;
}

std::string Dataset::attachmentsTableName(const std::string &name) const
{
    return NG_PREFIX + name + "_" + ATTACH_SUFFIX;
}

bool Dataset::deleteFeatures(const std::string &name)
{
    GDALDatasetPtr ds;
    if(m_DS->GetLayerByName(name.c_str()) != nullptr) {
        ds = m_DS;
    }

    if(!ds && m_addsDS->GetLayerByName(name.c_str()) != nullptr) {
        ds = m_addsDS;
    }

    if(!ds) {
        return false;
    }
    resetError();
    MutexHolder holder(m_executeSQLMutex);
    ds->ExecuteSQL(CPLSPrintf("DELETE from %s", name.c_str()), nullptr, nullptr);
    return CPLGetLastErrorType() < CE_Failure;
}

void Dataset::releaseResultSet(Table *table)
{
    if(m_DS && nullptr != table) {
        m_DS->ReleaseResultSet(table->m_layer);
    }
}

void Dataset::refresh()
{
    if(!m_childrenLoaded) {
        loadChildren();
        return;
    }

    std::vector<std::string> deleteNames;
    std::vector<std::string> addNames;
    for(int i = 0; i < m_DS->GetLayerCount(); ++i) {
        OGRLayer *layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            // Hide metadata, overviews, history tables
            if(skipFillFeatureClass(layer)) {
                continue;
            }
            const char *layerName = layer->GetName();
            CPLDebug("ngstore", "refresh layer %s", layerName);
            addNames.emplace_back(layerName);
        }
    }

    // Fill delete names array
    for(const ObjectPtr &child : m_children) {
        CPLDebug("ngstore", "refresh del layer %s", child->name().c_str());
        deleteNames.push_back(child->name());
    }

    // Remove same names from add and delete arrays
    removeDuplicates(deleteNames, addNames);

    CPLDebug("ngstore", "Add count %ld, delete count %ld", addNames.size(), deleteNames.size());

    // Delete objects
    auto it = m_children.begin();
    while(it != m_children.end()) {
        std::string name = (*it)->name();
        auto itdn = std::find(deleteNames.begin(), deleteNames.end(), name);
        if(itdn != deleteNames.end()) {
            deleteNames.erase(itdn);
            it = m_children.erase(it);
        }
        else {
            ++it;
        }
    }

    // Create new objects
    for(const auto &layerName : addNames) {
        OGRLayer *layer = m_DS->GetLayerByName(layerName.c_str());
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new Table(layer, this,
                        CAT_TABLE_ANY, layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new FeatureClass(layer, this,
                            CAT_FC_ANY, layerName)));
            }
        }
    }
}

bool Dataset::loadChildren()
{
    if(m_childrenLoaded) {
        return true;
    }

    if(!isOpened()) {
        if(!open()) {
            return false;
        }
    }

    // fill vector layers and tables
    fillFeatureClasses();

    // fill rasters
    char **subdatasetList = m_DS->GetMetadata("SUBDATASETS");
    std::vector<std::string> siblingFiles;
    if(nullptr != subdatasetList) {
        int i = 0;
        size_t strLen = 0;
        const char* testStr = nullptr;
        char rasterPath[255];
        while((testStr = subdatasetList[i++]) != nullptr) {
            strLen = CPLStrnlen(testStr, 255);
            if(compare(testStr + strLen - 4, "NAME")) {
                CPLStrlcpy(rasterPath, testStr, strLen - 4);
                CPLStringList pathPortions(CSLTokenizeString2( rasterPath, ":", 0 ));
                const char *rasterName = pathPortions[pathPortions.size() - 1];
                Dataset *parent = const_cast<Dataset *>(this);
                m_children.push_back(ObjectPtr(new Raster(siblingFiles, parent,
                    CAT_RASTER_ANY, rasterName, rasterPath)));
            }
        }
        CSLDestroy(subdatasetList);
    }

    m_childrenLoaded = true;

    return true;
}

//------------------------------------------------------------------------------
// DatasetBatchOperationHolder
//------------------------------------------------------------------------------

DatasetBatchOperationHolder::DatasetBatchOperationHolder(Dataset *dataset) :
    m_dataset(dataset)
{
    if(nullptr != m_dataset)
        m_dataset->startBatchOperation();
}

DatasetBatchOperationHolder::~DatasetBatchOperationHolder()
{
    if(nullptr != m_dataset)
        m_dataset->stopBatchOperation();
}

//------------------------------------------------------------------------------
// DatasetExecuteSQLLockHolder
//------------------------------------------------------------------------------

DatasetExecuteSQLLockHolder::DatasetExecuteSQLLockHolder(Dataset* dataset) :
    m_dataset(dataset)
{
    if(nullptr != m_dataset)
        m_dataset->lockExecuteSql(true);
}

DatasetExecuteSQLLockHolder::~DatasetExecuteSQLLockHolder()
{
    if(nullptr != m_dataset)
        m_dataset->lockExecuteSql(false);
}

} // namespace ngs
