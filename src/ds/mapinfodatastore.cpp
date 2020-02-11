/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019 NextGIS, <info@nextgis.com>
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

#include "mapinfodatastore.h"

#include "catalog/file.h"
#include "catalog/folder.h"
#include "ngstore/catalog/filter.h"

#include "util.h"
#include "util/error.h"
#include "util/notify.h"

#include "ngstore/version.h"


namespace ngs {

constexpr const char *STORE_EXT = "ngmi"; // NextGIS MapInfo Store
//constexpr int STORE_EXT_LEN = length(STORE_EXT);
constexpr const char *STORE_META_DB = "sys.db";
constexpr const char *HASH_SUFFIX = "hash";
constexpr const char *HASH_FIELD = "hash";

// -----------------------------------------------------------------------------
static const std::vector<std::string> tabExts = {"dat", "map", "id", "ind",
                                                 "cpg", "qix", "osf"};
static bool deleteTab(const std::string &path) {
    if(!File::deleteFile(path)) {
        return false;
    }
    // Try extensions list
    for(const auto& ext : tabExts) {
        auto delPath = File::resetExtension(path, ext);
        if(Folder::isExists(delPath)) {
            File::deleteFile(delPath);
        }
    }
    return true;
}


static std::string hashTableName(const std::string &name)
{
    return NG_PREFIX + name + "_" + HASH_SUFFIX;
}

static OGRLayer *createHashTableInt(GDALDataset *ds, const std::string &name)
{
    resetError();
    OGRLayer *hashLayer = ds->CreateLayer(name.c_str(), nullptr,
                                          wkbNone, nullptr);
    if (nullptr == hashLayer) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    // Add id field
    OGRFieldDefn fidField(FEATURE_ID_FIELD, OFTInteger64);

    // Add row hash field
    OGRFieldDefn hashField(HASH_FIELD, OFTString);
    hashField.SetWidth(64);

    OGRFieldDefn ridField(ngw::REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, ngw::INIT_RID_COUNTER));

    // Create table  fields
    if(hashLayer->CreateField(&fidField) != OGRERR_NONE ||
       hashLayer->CreateField(&hashField) != OGRERR_NONE ||
       hashLayer->CreateField(&ridField) != OGRERR_NONE) {
        outMessage(COD_CREATE_FAILED, CPLGetLastErrorMsg());
        return nullptr;
    }

    return hashLayer;
}

static bool isChildExists(const std::string &checkPath,
                          const std::vector<ObjectPtr> &list)
{
    for(const auto &item : list) {
        if(item->path() == checkPath) {
            return true;
        }
    }
    return false;
}


//------------------------------------------------------------------------------
// MapInfoStoreTable
//------------------------------------------------------------------------------
MapInfoStoreTable::MapInfoStoreTable(GDALDataset *DS, OGRLayer *layer,
                                     ObjectContainer * const parent,
                                     const std::string &path,
                                     const std::string &encoding) :
    Table(layer, parent, CAT_TABLE_MAPINFO_TAB, ""),
    StoreObject(layer),
    m_TABDS(DS),
    m_encoding(encoding)
{
    m_path = path;
    m_storeName = File::getFileName(path);
    if(nullptr != layer) {
        m_name = layer->GetMetadataItem(DESCRIPTION_KEY);
    }
//    if(m_name.empty()) {
//        m_name = property(DESCRIPTION_KEY, "", NG_ADDITIONS_KEY);
//    }
    if(m_name.empty()) {
        m_name = File::getBaseName(m_path);
    }
}

MapInfoStoreTable::~MapInfoStoreTable()
{
    close();
}

Properties MapInfoStoreTable::properties(const std::string &domain) const
{
    auto out = Table::properties(domain);
    if(m_TABDS) {
        out.add(READ_ONLY_KEY, m_TABDS->GetAccess() == GA_ReadOnly ? "ON" : "OFF");
        if(m_layer) {
            out.add(DESCRIPTION_KEY, m_layer->GetMetadataItem(DESCRIPTION_KEY));
        }
    }
    return out;
}

std::string MapInfoStoreTable::property(const std::string &key,
                                        const std::string &defaultValue,
                                        const std::string &domain) const
{
    if(compare(key, READ_ONLY_KEY) && compare(domain, NG_ADDITIONS_KEY)) {
        if(m_TABDS) {
            return m_TABDS->GetAccess() == GA_ReadOnly ? "ON" : "OFF";
        }
    }
    else if(m_layer && compare(key, DESCRIPTION_KEY) &&
            compare(domain, NG_ADDITIONS_KEY)) {
        return m_layer->GetMetadataItem(DESCRIPTION_KEY);
    }
    return Table::property(key, defaultValue, domain);
}

bool MapInfoStoreTable::destroy()
{
    close();
    return deleteTab(m_path);
}

std::string MapInfoStoreTable::storeName() const
{
    return m_storeName;
}

bool MapInfoStoreTable::checkSetProperty(const std::string &key,
                                         const std::string &value,
                                         const std::string &domain)
{
    if(compare(key, READ_ONLY_KEY) && compare(domain, NG_ADDITIONS_KEY)) {
        bool readOnly = toBool(value);
        close();
        m_TABDS = static_cast<GDALDataset*>(GDALOpenEx(m_path.c_str(),
                GDAL_OF_SHARED|readOnly ? GDAL_OF_READONLY : GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR,
                                               nullptr, nullptr, nullptr));
        if(!m_TABDS) {
            return false;
        }
        m_layer = m_TABDS->GetLayer(0);
        return m_layer != nullptr && Table::checkSetProperty(key, value, domain);
    }
    else if(m_layer && compare(key, DESCRIPTION_KEY) &&
            compare(domain, NG_ADDITIONS_KEY)) {
        return m_layer->SetMetadataItem(DESCRIPTION_KEY, value.c_str()) == CE_None &&
                Table::checkSetProperty(key, value, domain);
    }
    return Table::checkSetProperty(key, value, domain);
}

bool MapInfoStoreTable::sync(const Options &options)
{
    auto isSyncStr = property(ngw::SYNC_KEY, "OFF", NG_ADDITIONS_KEY);
    if(!toBool(isSyncStr)) {
        return true; // No sync
    }

    return true;
}

void MapInfoStoreTable::close()
{
    GDALClose(m_TABDS);
    m_TABDS = nullptr;
    m_layer = nullptr;
}

//------------------------------------------------------------------------------
// MapInfoStoreFeatureClass
//------------------------------------------------------------------------------
MapInfoStoreFeatureClass::MapInfoStoreFeatureClass(GDALDataset *DS, OGRLayer *layer,
                                    ObjectContainer * const parent,
                                    const std::string &path,
                                    const std::string &encoding) :
    FeatureClass(layer, parent, CAT_FC_MAPINFO_TAB, ""),
    StoreObject(layer),
    m_TABDS(DS),
    m_encoding(encoding)
{
    m_path = path;
    m_storeName = File::getFileName(path);
    if(nullptr != layer) {
        m_name = layer->GetMetadataItem(DESCRIPTION_KEY);
    }
    if(m_name.empty()) {
        m_name = File::getBaseName(m_path);
    }
}

MapInfoStoreFeatureClass::~MapInfoStoreFeatureClass()
{
    close();
}

Properties MapInfoStoreFeatureClass::properties(const std::string &domain) const
{
    auto out = FeatureClass::properties(domain);
    if(m_TABDS) {
        out.add(READ_ONLY_KEY, Dataset::isReadOnly(m_TABDS) ? "ON" : "OFF");
        if(m_layer) {
            out.add(DESCRIPTION_KEY, m_layer->GetMetadataItem(DESCRIPTION_KEY));
        }
    }
    return out;
}

std::string MapInfoStoreFeatureClass::property(const std::string &key,
                                               const std::string &defaultValue,
                                               const std::string &domain) const
{
    if(compare(key, READ_ONLY_KEY) && compare(domain, NG_ADDITIONS_KEY)) {
        if(m_TABDS) {
            return Dataset::isReadOnly(m_TABDS) ? "ON" : "OFF";
        }
    }
    else if(m_layer && compare(key, DESCRIPTION_KEY) &&
            compare(domain, NG_ADDITIONS_KEY)) {
        return m_layer->GetMetadataItem(DESCRIPTION_KEY);
    }
    return FeatureClass::property(key, defaultValue, domain);
}

bool MapInfoStoreFeatureClass::destroy()
{
    close();
    return deleteTab(m_path);
}

std::string MapInfoStoreFeatureClass::storeName() const
{
    return m_storeName;
}

bool MapInfoStoreFeatureClass::checkSetProperty(const std::string &key,
                                                const std::string &value,
                                                const std::string &domain)
{
    if(compare(key, READ_ONLY_KEY) && compare(domain, NG_ADDITIONS_KEY)) {
        bool readOnly = toBool(value);
        close();
        m_TABDS = static_cast<GDALDataset*>(GDALOpenEx(m_path.c_str(),
                GDAL_OF_SHARED|readOnly ? GDAL_OF_READONLY : GDAL_OF_UPDATE|GDAL_OF_VERBOSE_ERROR,
                                               nullptr, nullptr, nullptr));
        if(!m_TABDS) {
            return false;
        }
        m_layer = m_TABDS->GetLayer(0);
        return m_layer != nullptr && FeatureClass::checkSetProperty(key, value,
                                                                    domain);
    }
    else if(m_layer && compare(key, DESCRIPTION_KEY) &&
            compare(domain, NG_ADDITIONS_KEY)) {
        return m_layer->SetMetadataItem(DESCRIPTION_KEY, value.c_str()) == CE_None &&
                FeatureClass::checkSetProperty(key, value, domain);
    }
    return FeatureClass::checkSetProperty(key, value, domain);
}

bool MapInfoStoreFeatureClass::insertFeature(const FeaturePtr &feature, bool logEdits)
{
    MapInfoDataStore *parentDS = dynamic_cast<MapInfoDataStore*>(m_parent);
    if(nullptr != parentDS) {
        resetError();
        auto hashTable  = parentDS->getHashTable(name());
        if(hashTable) {
            auto hashFeature = OGRFeature::CreateFeature(hashTable->GetLayerDefn());
            hashFeature->SetField(FEATURE_ID_FIELD, feature->GetFID());
            hashFeature->SetField(
                HASH_FIELD, feature.dump(FeaturePtr::DumpOutputType::HASH_STYLE).c_str());
        }
    }
    return FeatureClass::insertFeature(feature, logEdits);
}

static FeaturePtr getFeatureByRID(OGRLayer *lyr, GIntBig fid)
{
    if(nullptr == lyr) {
        return FeaturePtr();
    }
    lyr->SetAttributeFilter(CPLSPrintf("%s = " CPL_FRMT_GIB, FEATURE_ID_FIELD, fid));
    FeaturePtr out(lyr->GetNextFeature());
    lyr->SetAttributeFilter(nullptr);
    return out;
}

bool MapInfoStoreFeatureClass::updateFeature(const FeaturePtr &feature, bool logEdits)
{
    MapInfoDataStore *parentDS = dynamic_cast<MapInfoDataStore*>(m_parent);
    if(nullptr != parentDS) {
        resetError();
        auto hashTable  = parentDS->getHashTable(name());
        auto hashFeature = getFeatureByRID(hashTable, feature->GetFID());
        if(hashFeature) {
            hashFeature->SetField(
                HASH_FIELD, feature.dump(FeaturePtr::DumpOutputType::HASH_STYLE).c_str());
            if(hashTable->SetFeature(hashFeature) != OGRERR_NONE) {
                return errorMessage("Update feature failed. Error: %s",
                                    CPLGetLastErrorMsg());
            }
        }
    }
    return FeatureClass::updateFeature(feature, logEdits);
}

bool MapInfoStoreFeatureClass::deleteFeature(GIntBig id, bool logEdits)
{
    MapInfoDataStore *parentDS = dynamic_cast<MapInfoDataStore*>(m_parent);
    if(nullptr != parentDS) {
        resetError();
        auto hashTable  = parentDS->getHashTable(name());
        auto feature = getFeatureByRID(hashTable, id);
        if(feature) {
            if(hashTable->DeleteFeature(feature->GetFID()) != OGRERR_NONE) {
                return errorMessage("Delete feature failed. Error: %s",
                                    CPLGetLastErrorMsg());
            }
        }
    }
    return FeatureClass::deleteFeature(id, logEdits);
}

bool MapInfoStoreFeatureClass::deleteFeatures(bool logEdits)
{
    MapInfoDataStore *parentDS = dynamic_cast<MapInfoDataStore*>(m_parent);
    if(nullptr != parentDS) {
        parentDS->clearHashTable(name());
    }
    return FeatureClass::deleteFeatures(logEdits);
}

std::vector<ngsEditOperation> MapInfoStoreFeatureClass::editOperations()
{
    updateHashAndEditLog();
    return FeatureClass::editOperations();
}

bool MapInfoStoreFeatureClass::onRowsCopied(const Progress &progress,
                                            const Options &options)
{
    bool createHash = options.asBool(ngw::SYNC_KEY, false);
    if(createHash) {
        if(!setProperty(LOG_EDIT_HISTORY_KEY, "ON", NG_ADDITIONS_KEY)) {
            return false;
        }
        return fillHash(progress, options);
    }
    return FeatureClass::onRowsCopied(progress, options);
}

bool MapInfoStoreFeatureClass::sync(const Options &options)
{
    auto isSyncStr = property(ngw::SYNC_KEY, "OFF", NG_ADDITIONS_KEY);
    if(!toBool(isSyncStr)) {
        return true; // No sync
    }
    return true;
}

void MapInfoStoreFeatureClass::close()
{
    GDALClose(m_TABDS);
    m_TABDS = nullptr;
    m_layer = nullptr;
}

int MapInfoStoreFeatureClass::fillHash(const Progress &progress,
                                        const Options &options)
{
    ngsUnused(options);
    MapInfoDataStore *parentDS = dynamic_cast<MapInfoDataStore*>(m_parent);
    if(nullptr == parentDS) {
        progress.onProgress(COD_CREATE_FAILED, 0.0,
                            _("Unsupported feature class"));
        return errorMessage(_("Unsupported feature class"));
    }

    auto hashTable  = parentDS->getHashTable(name());

    if(nullptr == hashTable) {
        hashTable = parentDS->createHashTable(name());
    }
    else {
        parentDS->clearHashTable(name());
    }

    // Hash features.

    emptyFields(true);
    reset();
    progress.onProgress(COD_IN_PROCESS, 0.0, _("Start hashing features"));
    double counter(0.0);
    auto featureCountVal = featureCount();
    FeaturePtr feature;
    while((feature = nextFeature())) {
        double complete = counter / featureCountVal;
        if(!progress.onProgress(COD_IN_PROCESS, complete,
                                _("Hash in process ..."))) {
            return  COD_CANCELED;
        }
        auto hash = feature.dump(FeaturePtr::DumpOutputType::HASH_STYLE);
        FeaturePtr newFeature = OGRFeature::CreateFeature(
                    hashTable->GetLayerDefn() );
        newFeature->SetField(FEATURE_ID_FIELD, feature->GetFID());
        newFeature->SetField(HASH_FIELD, hash.c_str());
        if(hashTable->CreateFeature(newFeature) != OGRERR_NONE) {
            outMessage(COD_INSERT_FAILED, _("Failed to create feature"));
        }
    }

    progress.onProgress(COD_FINISHED, 1.0, _("Hashing features finished"));

    emptyFields(false);
    reset();

    return true;
}

bool MapInfoStoreFeatureClass::updateHashAndEditLog()
{
    MapInfoDataStore *parentDS = dynamic_cast<MapInfoDataStore*>(m_parent);
    if(nullptr == parentDS) {
        return false; // Should never happen.
    }

    auto hashTable  = parentDS->getHashTable(name());
    if(nullptr == hashTable) {
        return true; // Hash table is not exists. Should never happen.
    }

    resetError();

    FeaturePtr feature;
    hashTable->ResetReading();
    std::vector<GIntBig> deleteIDs;
    std::vector<GIntBig> presentIDs;
    while((feature = hashTable->GetNextFeature())) {
        // Check update or delete
        auto fid = feature->GetFieldAsInteger64(FEATURE_ID_FIELD);
        auto rid = feature->GetFieldAsInteger64(ngw::REMOTE_ID_KEY);
        auto tabFeature = getFeature(fid);
        // Check update
        if(tabFeature) {
            auto storedHash = feature->GetFieldAsString(HASH_FIELD);
            auto currentHash = tabFeature.dump(FeaturePtr::DumpOutputType::HASH_STYLE);
            if(!compare(storedHash, currentHash)) {
                FeaturePtr opFeature = logEditFeature(FeaturePtr(), FeaturePtr(),
                                                  CC_CHANGE_FEATURE);
                opFeature->SetField(FEATURE_ID_FIELD, fid);
                opFeature->SetField(ngw::REMOTE_ID_KEY, rid);

                logEditOperation(opFeature);

                // Update hash
                feature->SetField(HASH_FIELD, currentHash.c_str());
                if (hashTable->SetFeature(feature) != OGRERR_NONE) {
                    warningMessage(_("Failed to save new hash for feature " CPL_FRMT_GIB),
                                   tabFeature->GetFID());
                }
            }
            presentIDs.push_back(fid);
        }
        else { // Feature deleted
            deleteIDs.push_back(feature->GetFID());
            FeaturePtr opFeature = logEditFeature(FeaturePtr(), FeaturePtr(),
                                                  CC_DELETE_FEATURE);
            opFeature->SetField(FEATURE_ID_FIELD, fid);
            opFeature->SetField(ngw::REMOTE_ID_KEY, rid);
            logEditOperation(opFeature);
        }
    }

    for(auto deleteID : deleteIDs) {
        if(hashTable->DeleteFeature(deleteID) != OGRERR_NONE) {
            warningMessage("Failed delete hash table item " CPL_FRMT_GIB, deleteID);
        }
    }

    // Check add
    reset();
    while((feature = nextFeature())) {
        if(std::find(presentIDs.begin(), presentIDs.end(), feature->GetFID()) ==
                presentIDs.end() ) {
            // New feature added
            FeaturePtr opFeature = logEditFeature(FeaturePtr(), FeaturePtr(),
                                                  CC_CREATE_FEATURE);
            opFeature->SetField(FEATURE_ID_FIELD, feature->GetFID());
            logEditOperation(opFeature);
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// MapInfoDataStore
//------------------------------------------------------------------------------

MapInfoDataStore::MapInfoDataStore(ObjectContainer * const parent,
                                   const std::string &name,
                                   const std::string &path) :
                  Dataset(parent, CAT_CONTAINER_MAPINFO_STORE, name, path),
                  SpatialDataset()
{
    m_spatialReference = SpatialReferencePtr::importFromEPSG(DEFAULT_EPSG);
}


bool MapInfoDataStore::create(const std::string &path)
{
    resetError();
    if(path.empty()) {
        return errorMessage(_("The path is empty"));
    }

    std::string newPath = path;
    if(!endsWith(path, extension())) {
        newPath += "." + extension();
    }

    if(!Folder::mkDir(newPath, true)) {
        return false;
    }

    GDALDriver *driver = Filter::getGDALDriver(CAT_CONTAINER_MAPINFO_STORE);
    if(driver == nullptr) {
        return errorMessage(_("Driver is not present"));
    }

    std::string dsPath = File::formFileName(newPath, STORE_META_DB);
    Options options;
    options.add("METADATA", "NO");
    options.add("SPATIALITE", "NO");
    options.add("INIT_WITH_EPSG", "NO");

    GDALDataset *DS = driver->Create(dsPath.c_str(), 0, 0, 0, GDT_Unknown,
                                     options.asCPLStringList());
    if(DS == nullptr) {
        return errorMessage(_("Failed to create datastore. %s"), CPLGetLastErrorMsg());
    }

    createMetadataTable(DS);

    GDALClose(DS);
    return true;
}

std::string MapInfoDataStore::extension()
{
    return STORE_EXT;
}

bool MapInfoDataStore::destroy()
{
    close();
    return Folder::rmDir(m_path);
}

bool MapInfoDataStore::open(unsigned int openFlags, const Options &options)
{
    if(isOpened()) {
        return true;
    }

    std::string dsPath = "SQLITE:" + File::formFileName(m_path, STORE_META_DB);
    bool result = DatasetBase::open(dsPath, openFlags|GDAL_OF_VECTOR, options);
    if(result) {
        m_addsDS = m_DS;
        m_addsDS->Reference();

        m_metadata = m_addsDS->GetLayerByName(METADATA_TABLE_NAME);
    }
    else {
        return result;
    }

    resetError();

    int version = std::stoi(property(NGS_VERSION_KEY, "0", NG_ADDITIONS_KEY));
    if(version < NGS_VERSION_NUM && !upgrade(version)) {
        return errorMessage(_("Upgrade storage failed"));
    }
    return true;
}

bool MapInfoDataStore::upgrade(int oldVersion)
{
    executeSQL("VACUUM", "SQLite");
    ngsUnused(oldVersion);
    // no structure changes for version 1
    return true;
}

OGRLayer *MapInfoDataStore::getHashTable(const std::string &name)
{
    if(!m_addsDS) {
        return nullptr;
    }
    return m_addsDS->GetLayerByName(hashTableName(name).c_str());
}

OGRLayer *MapInfoDataStore::createHashTable(const std::string &name)
{
    if(!m_addsDS) {
        createAdditionsDataset();
    }

    if(!m_addsDS) {
        return nullptr;
    }

    return createHashTableInt(m_addsDS, hashTableName(name));
}

void MapInfoDataStore::clearHashTable(const std::string &name)
{
    deleteFeatures(hashTableName(name));
}

bool MapInfoDataStore::destroyHashTable(const std::string &name)
{
    OGRLayer *layer = getHashTable(name);
    if(!layer) {
        return false;
    }
    return destroyTable(m_addsDS, layer);
}

bool MapInfoDataStore::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly()) {
        return false;
    }
    return type == CAT_FC_MAPINFO_TAB; // NGW cannot store plain tables now. Disable it temporary. || type == CAT_TABLE_MAPINFO_TAB;
}


ObjectPtr MapInfoDataStore::create(const enum ngsCatalogObjectType type,
    const std::string &name, const Options& options)
{
    bool overwrite = options.asBool("OVERWRITE", false);
    if (overwrite) {
        ObjectPtr deleteObject = getChild(name);
        if (deleteObject) {
            if(!deleteObject->destroy()) {
                errorMessage(_("Failed overwrite existing object %s. Error: %s"),
                    name.c_str(), CPLGetLastErrorMsg());
                return ObjectPtr();
            }
        }
    }
    std::string newName = normalizeDatasetName(name);

    // Get field definitions
    CREATE_FEATURE_DEFN_RESULT featureDefnStruct =
            createFeatureDefinition(name, options);

    ObjectPtr object;
    if(type == CAT_FC_MAPINFO_TAB) {
        OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(
                    options.asString("GEOMETRY_TYPE", ""));
        if((geomType < wkbPoint || geomType > wkbMultiPolygon) &&
           (geomType < wkbPoint25D || geomType > wkbMultiPolygon25D)) {
            errorMessage(_("Unsupported geometry type"));
            return ObjectPtr();
        }

        object = ObjectPtr(createFeatureClass(newName, CAT_FC_MAPINFO_TAB,
                                              featureDefnStruct.defn.get(),
                                              m_spatialReference, geomType,
                                              options));
    }
    else if(type == CAT_TABLE_MAPINFO_TAB) {
        return ObjectPtr(); // Not support yet.
//        object = ObjectPtr(createTable(newName, CAT_TABLE_MAPINFO_TAB,
//                                       featureDefnStruct.defn,
//                                       options));
    }

    // Error occured
    if(!object) {
        return object;
    }

    setMetadata(object, featureDefnStruct.fields, options);
    m_children.push_back(object);

    return object;
}

OGRLayer *MapInfoDataStore::createAttachmentsTable(const std::string &name)
{
    if(!m_addsDS) {
        createAdditionsDataset();
    }

    if(!m_addsDS) {
        return nullptr;
    }

    std::string attLayerName(attachmentsTableName(name));
    return ngw::createAttachmentsTable(m_addsDS, attLayerName, m_path);
}

OGRLayer *MapInfoDataStore::createEditHistoryTable(const std::string &name)
{
    if(!m_addsDS) {
        createAdditionsDataset();
    }

    if(!m_addsDS) {
        return nullptr;
    }

    std::string logLayerName(historyTableName(name));
    return ngw::createEditHistoryTable(m_addsDS, logLayerName);
}

void MapInfoDataStore::fillFeatureClasses() const
{
    MapInfoDataStore *parent = const_cast<MapInfoDataStore*>(this);
    auto filesList = Folder::listFiles(m_path);
    auto encoding = property("ENCODING", "CP1251", NG_ADDITIONS_KEY);
    for(const auto &fileItem : filesList) {
        if(compare(File::getExtension(fileItem), "tab")) {
            auto path = File::formFileName(m_path, fileItem);
            if(isChildExists(path, m_children)) {
                continue;
            }
            GDALDataset *DS = static_cast<GDALDataset*>(
                        GDALOpenEx(path.c_str(),
                                   GDAL_OF_SHARED|GDAL_OF_READONLY|GDAL_OF_VERBOSE_ERROR,
                                   nullptr, nullptr, nullptr));
            OGRLayer *layer = DS->GetLayer(0);

            auto geomTypeNameStr = property("GEOMETRY_TYPE", "", fileItem + "." +
                                            NG_ADDITIONS_KEY);

            OGRwkbGeometryType geometryType;
            if(geomTypeNameStr.empty()) {
                geometryType = layer->GetGeomType();
            }
            else {
                geometryType = FeatureClass::geometryTypeFromName(geomTypeNameStr);
            }
            if(geometryType == wkbNone) {
                m_children.push_back(
                            ObjectPtr(new MapInfoStoreTable(
                                          DS, layer, parent, path, encoding)));
            }
            else {
                m_children.push_back(
                            ObjectPtr(new MapInfoStoreFeatureClass(
                                          DS, layer, parent, path, encoding)));
            }
        }
    }
}

std::string MapInfoDataStore::normalizeFieldName(const std::string &name,
                                                 const std::vector<std::string> &nameList,
                                                 int counter) const
{
    auto out = Dataset::normalizeFieldName(name, nameList, counter);
    // 31 char max
    int extra = int(out.size()) - 31;
    if(extra <= 0) {
        return out;
    }

    auto newName = name.substr(0, name.size() - unsigned(extra));
    return normalizeFieldName(newName, nameList, counter);
}

Table *MapInfoDataStore::createTable(const std::string& name,
                               enum ngsCatalogObjectType objectType,
                               OGRFeatureDefn * const definition,
                               const Options &options, const Progress &progress)
{
    resetError();
    ngsUnused(objectType);

    GDALDriver *driver = Filter::getGDALDriver(CAT_TABLE_MAPINFO_TAB);
    if(nullptr == driver) {
        errorMessage(_("Driver not available. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    auto optionsList = options.asCPLStringList();
    auto encoding = options.asString("ENCODING", "");
    if(encoding.empty()) {
        encoding = property("ENCODING", "CP1251", NG_ADDITIONS_KEY);
    }
    if(!encoding.empty()) {
        optionsList.AddNameValue("ENCODING", encoding.c_str());
    }
    auto path = File::formFileName(m_path, name, "tab");
    GDALDataset *DS = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, optionsList);
    if(nullptr == DS) {
        errorMessage(_("Create of %s file failed. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    OGRLayer *layer = DS->CreateLayer(name.c_str(), nullptr, wkbNone, optionsList);
    if(layer == nullptr) {
        errorMessage(_("Failed to create table %s. %s"), name.c_str(), CPLGetLastErrorMsg());
        return nullptr;
    }

    std::vector<std::string> nameList;
    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        std::string newFieldName = normalizeFieldName(srcField->GetNameRef(),
                                                      nameList);
        if(!compare(newFieldName, srcField->GetNameRef())) {
            progress.onProgress(COD_WARNING, 0.0,
                _("Field %s of source table was renamed to %s in destination tables"),
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

    Table *out = new MapInfoStoreTable(DS, layer, this, path, encoding);

    // Set description
    auto description = options.asString(DESCRIPTION_KEY);
    if(!description.empty()) {
        out->setProperty(DESCRIPTION_KEY, description, NG_ADDITIONS_KEY);
    }

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->fullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

FeatureClass *MapInfoDataStore::createFeatureClass(const std::string &name,
                                             enum ngsCatalogObjectType objectType,
                                             OGRFeatureDefn * const definition,
                                             SpatialReferencePtr spatialRef,
                                             OGRwkbGeometryType type,
                                             const Options &options,
                                             const Progress &progress)
{
    resetError();
    ngsUnused(objectType);

    GDALDriver *driver = Filter::getGDALDriver(CAT_FC_MAPINFO_TAB);
    if(nullptr == driver) {
        errorMessage(_("Driver not available. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    auto optionsList = options.asCPLStringList();
    auto defaultEncoding = property("ENCODING", "CP1251", NG_ADDITIONS_KEY);
    auto encoding = options.asString("ENCODING", defaultEncoding);
    if(!encoding.empty()) {
        optionsList.AddNameValue("ENCODING", encoding.c_str());
    }
    auto path = File::formFileName(m_path, name, "tab");
    GDALDataset *DS = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, optionsList);
    if(nullptr == DS) {
        errorMessage(_("Create of %s file failed. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    OGRLayer *layer = DS->CreateLayer(name.c_str(), spatialRef, type, optionsList);
    if(layer == nullptr) {
        errorMessage(_("Failed to create table %s. %s"), name.c_str(), CPLGetLastErrorMsg());
        return nullptr;
    }

    std::vector<std::string> nameList;
    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        std::string newFieldName = normalizeFieldName(srcField->GetNameRef(),
                                                      nameList);
        if(!compare(newFieldName, srcField->GetNameRef())) {
            progress.onProgress(COD_WARNING, 0.0,
                _("Field %s of source table was renamed to %s in destination tables"),
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

    FeatureClass *out = new MapInfoStoreFeatureClass(DS, layer, this, path, encoding);

    // Set description
    auto description = options.asString(DESCRIPTION_KEY);
    if(!description.empty()) {
        out->setProperty(DESCRIPTION_KEY, description, NG_ADDITIONS_KEY);
    }

    out->setProperty("GEOMETRY_TYPE", FeatureClass::geometryTypeName(type,
                        FeatureClass::GeometryReportType::OGC),
                     NG_ADDITIONS_KEY);

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->fullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

} // namespace ngs
