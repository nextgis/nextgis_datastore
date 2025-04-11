/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2019-2025 NextGIS, <info@nextgis.com>
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

#include "catalog/catalog.h"
#include "catalog/file.h"
#include "catalog/folder.h"
#include "catalog/ngw.h"
#include "ds/ngw.h"
#include "ngstore/catalog/filter.h"
#include "ngstore/version.h"

#include "util.h"
#include "util/error.h"
#include "util/notify.h"
#include "util/url.h"

#include <ctime>


namespace ngs {

constexpr const char *STORE_EXT = "ngmi"; // NextGIS MapInfo Store
//constexpr int STORE_EXT_LEN = length(STORE_EXT);
constexpr const char *STORE_META_DB = "sys.db";
constexpr const char *HASH_SUFFIX = "hash";
constexpr const char *HASH_FIELD = "hash";
constexpr const char *STATE_CLOSE = "close";
constexpr const char *STATE_RO = "read-only";
constexpr const char *STATE_RW = "read-write";

// -----------------------------------------------------------------------------
static const std::vector<std::string> tabExts = {"dat", "map", "id", "ind",
                                                 "cpg", "qix", "osf"};
static bool deleteTab(const std::string &path) {
    if(!File::deleteFile(path)) {
        return false;
    }
    // Try extensions list
    for(const auto &ext : tabExts) {
        auto delPath = File::resetExtension(path, ext);
        if(Folder::isExists(delPath)) {
            File::deleteFile(delPath);
        }
    }
    return true;
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
MapInfoStoreTable::MapInfoStoreTable(GDALDatasetPtr DS,
                                     OGRLayer *layer,
                                     ObjectContainer * const parent,
                                     const std::string &path,
                                     const std::string &encoding) :
    Table(layer, parent, CAT_TABLE_MAPINFO_TAB, ""),
    StoreObject(nullptr),
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

bool MapInfoStoreTable::destroy()
{
    close();
    if(!deleteTab(m_path)) {
        return false;
    }

    return Object::destroy();
}

std::string MapInfoStoreTable::storeName() const
{
    return m_storeName;
}


void MapInfoStoreTable::close()
{
    m_TABDS = nullptr;
    m_layer = nullptr;
}

//------------------------------------------------------------------------------
// MapInfoStoreFeatureClass
//------------------------------------------------------------------------------
MapInfoStoreFeatureClass::MapInfoStoreFeatureClass(GDALDatasetPtr DS,
                                                   OGRLayer *layer,
                                                   ObjectContainer * const parent,
                                                   const std::string &path,
                                                   const std::string &encoding) :
    FeatureClass(layer, parent, CAT_FC_MAPINFO_TAB, ""),
    StoreObject(nullptr),
    m_TABDS(DS),
    m_encoding(encoding)
{
    m_path = path;
    m_storeName = File::getBaseName(path);
    if(nullptr != layer) {
        m_name = layer->GetMetadataItem(DESCRIPTION_KEY);
    }
    if(m_name.empty()) {
        m_name = File::getBaseName(m_path);
    }
}

bool MapInfoStoreFeatureClass::destroy()
{
    close();

    if(!deleteTab(m_path)) {
        return false;
    }

    return Object::destroy();
}

std::string MapInfoStoreFeatureClass::storeName() const
{
    return m_storeName;
}

void MapInfoStoreFeatureClass::onRowCopied(FeaturePtr srcFeature,
                                           FeaturePtr dstFature,
                                           const Options &options)
{
    auto sync = options.asString(ngw::SYNC_ATT_KEY, ngw::SYNC_DISABLE);
    auto maxSize = options.asLong(ngw::ATTACHMENTS_DOWNLOAD_MAX_SIZE, 0);
    if(maxSize == 0 && (compare(sync, ngw::SYNC_DISABLE) ||
                        compare(sync, ngw::SYNC_UPLOAD))) {
        return;
    }

    auto store = dynamic_cast<MapInfoDataStore*>(m_parent);
    auto attachments = srcFeature.attachments();
    for(const auto &attachment : attachments) {
        std::string path;
        if(attachment.size < maxSize) {
            path = store->tempPath();
            if(!http::getFile(attachment.path, path)) {
                path = ""; // Empty path means to download attachment on demand
            }
        }
        Options newOptions;
        newOptions.add(ngw::ATTACHMENT_REMOTE_ID_KEY, attachment.id);
        newOptions.add("MOVE", true);
        dstFature.addAttachment(attachment.name, attachment.description,
                                path, newOptions, false);
    }
}

bool MapInfoStoreFeatureClass::onRowsCopied(const TablePtr srcTable,
                                            const Progress &progress,
                                            const Options &options)

{
    // FIXME: do we need sync here? Just start syncing on user command or timer
    // if(m_TABDS) {
    //     m_TABDS->FlushCache();
    // }
    // auto sync = options.asString(ngw::SYNC_KEY, ngw::SYNC_DISABLE);
    // if(!compare(sync, ngw::SYNC_DISABLE)) {
    //     if(!setProperty(LOG_EDIT_HISTORY_KEY, "ON", NG_ADDITIONS_KEY)) {
    //         return false;
    //     }
    //     auto resource = ngsDynamicCast(NGWResourceBase, srcTable);
    //     if(nullptr == resource) {
    //         resource = dynamic_cast<NGWResourceBase*>(srcTable->parent());
    //         if(nullptr == resource) {
    //             warningMessage(_("Not NextGIS Web resource."));
    //             return true;
    //         }
    //     }


    //     // Write source table to properties
    //     if(!resource->isSyncable()) {
    //         warningMessage(_("Cannot sync resource %s"), srcTable->name().c_str());
    //     }
    //     else {
    //         NGWConnection *connection =
    //                 dynamic_cast<NGWConnection*>(resource->connection());
    //         if(nullptr != connection) {
    //             std::string syncResourceId = resource->resourceId();
    //             std::string connPath = connection->fullName();
    //             std::string syncAttachments = options.asString(ngw::SYNC_ATT_KEY,
    //                                                            ngw::SYNC_DISABLE);

    //             setProperty(ngw::NGW_ID, syncResourceId, NG_ADDITIONS_KEY);
    //             setProperty(ngw::NGW_CONNECTION, connPath, NG_ADDITIONS_KEY);
    //             setProperty(ngw::SYNC_KEY, sync, NG_ADDITIONS_KEY);
    //             setProperty(ngw::SYNC_ATT_KEY, syncAttachments,
    //                                  NG_ADDITIONS_KEY);
    //         }
    //     }

    //     return fillHash(progress, options);
    // }
    // auto logEdit = options.asBool(LOG_EDIT_HISTORY_KEY, false);
    // if(logEdit) {
    //     if(!setProperty(LOG_EDIT_HISTORY_KEY, "ON", NG_ADDITIONS_KEY)) {
    //         return false;
    //     }
    //     return fillHash(progress, options);
    // }
    return FeatureClass::onRowsCopied(srcTable, progress, options);
}


void MapInfoStoreFeatureClass::close()
{
    m_TABDS = nullptr;
    m_layer = nullptr;
}

//------------------------------------------------------------------------------
// MapInfoDataStore
//------------------------------------------------------------------------------

MapInfoDataStore::MapInfoDataStore(ObjectContainer * const parent,
                                   const std::string &name,
                                   const std::string &path) :
    Dataset(parent, CAT_CONTAINER_MAPINFO_STORE, name, path),
    SpatialDataset(SpatialReferencePtr::importFromEPSG(DEFAULT_EPSG))
{
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

    std::string dsPath = File::formFileName(newPath, STORE_META_DB);
    GDALDatasetPtr DS = Dataset::createAdditionsDatasetInt(dsPath,
                                                         CAT_CONTAINER_MAPINFO_STORE);

    if(DS == nullptr) {
        return errorMessage(_("Failed to create datastore. %s"), CPLGetLastErrorMsg());
    }

    createMetadataTable(DS);

    return true;
}

std::string MapInfoDataStore::extension()
{
    return STORE_EXT;
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
        m_metadata = m_addsDS->GetLayerByName(METADATA_TABLE_NAME);
    }
    else {
        return result;
    }

    resetError();

    int version = atoi(property(NGS_VERSION_KEY, "0", NG_ADDITIONS_KEY).c_str());
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

std::string MapInfoDataStore::tempPath() const
{
    auto tmpDir = File::formFileName(m_path, "tmp");
    if(!Folder::isExists(tmpDir)) {
        Folder::mkDir(tmpDir);
    }
    time_t rawTime = std::time(nullptr);

    return File::formFileName(tmpDir, std::to_string(rawTime));
}

bool MapInfoDataStore::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly(m_DS)) {
        return false;
    }
    return type == CAT_FC_MAPINFO_TAB; // NGW cannot store plain tables now. Disable it temporary. || type == CAT_TABLE_MAPINFO_TAB;
}


ObjectPtr MapInfoDataStore::create(const enum ngsCatalogObjectType type,
    const std::string &name, const Options& options)
{
    loadChildren();

    std::string newName = name;
    if(options.asBool("CREATE_UNIQUE")) {
        newName = createUniqueName(newName, false);
    }
    newName = normalizeDatasetName(name);

    ObjectPtr child = getChild(newName);
    if(child) {
        if(options.asBool("OVERWRITE", false)) {
            if(!child->destroy()) {
                errorMessage(_("Failed to overwrite %s\nError: %s"),
                    newName.c_str(), getLastError());
                return ObjectPtr();
            }
        }
        else {
            errorMessage(_("Resource %s already exists. Add overwrite option or create_unique option to create resource here"),
                newName.c_str());
            return ObjectPtr();
        }
    }
    child = nullptr;

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
        // TODO: Add lookup table support
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

int MapInfoDataStore::paste(ObjectPtr child, bool move, const Options &options,
                            const Progress &progress)
{
    Options newOptions(options);
    if(options.hasKey("NEW_NAME") && !options.hasKey(DESCRIPTION_KEY)) {
        newOptions.add(DESCRIPTION_KEY, child->name());
    }
    return Dataset::paste(child, move, newOptions, progress);
}

std::string MapInfoDataStore::additionsDatasetPath() const
{
    return File::formFileName(m_path, STORE_META_DB);
}

std::string MapInfoDataStore::attachmentsFolderPath(bool create) const
{
    auto attachmentsPath = File::formFileName(m_path, attachmentsFolderExtension());
    if(create && !Folder::isExists(attachmentsPath)) {
        Folder::mkDir(attachmentsPath, true);
    }
    return attachmentsPath;
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
    return ngw::createAttachmentsTable(m_addsDS, attLayerName);
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
    auto parent = const_cast<MapInfoDataStore*>(this);
    auto filesList = Folder::listFiles(m_path);
    auto encoding = property("ENCODING", "CP1251", NG_ADDITIONS_KEY);
    for(const auto &fileItem : filesList) {
        if(compare(File::getExtension(fileItem), "tab")) {
            auto path = File::formFileName(m_path, fileItem);
            if(isChildExists(path, m_children)) {
                continue;
            }
            GDALDatasetPtr DS = static_cast<GDALDataset*>(
                        GDALOpenEx(path.c_str(),
                                   GDAL_OF_SHARED|GDAL_OF_READONLY|GDAL_OF_VERBOSE_ERROR,
                                   nullptr, nullptr, nullptr));
            OGRLayer *layer = DS->GetLayer(0);
            if(nullptr == layer) {
                continue;
            }

            auto geomTypeNameStr = property("GEOMETRY_TYPE", "", fileItem + "." +
                                            NG_ADDITIONS_KEY);

            OGRwkbGeometryType geometryType;
            if(geomTypeNameStr.empty()) {
                geometryType = layer->GetGeomType();
            }
            else {
                geometryType = FeatureClass::geometryTypeFromName(geomTypeNameStr);
            }

            ObjectPtr object;
            if(geometryType == wkbNone) {
                auto tab = new MapInfoStoreTable(
                            DS, layer, parent, path, encoding);
                tab->close();
                object = ObjectPtr(tab);
            }
            else {
                auto tab = new MapInfoStoreFeatureClass(
                            DS, layer, parent, path, encoding);
                tab->close();
                object = ObjectPtr(tab);
            }
            if(object) {
                m_children.emplace_back(object);
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
    ngsUnused(objectType);
    resetError();

    GDALDriver *driver = Filter::getGDALDriver(CAT_TABLE_MAPINFO_TAB);
    if(nullptr == driver) {
        errorMessage(_("Driver not available. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    CPLStringList optionsList = options;
    auto encoding = options.asString("ENCODING", "");
    if(encoding.empty()) {
        encoding = property("ENCODING", "CP1251", NG_ADDITIONS_KEY);
    }
    if(!encoding.empty()) {
        optionsList.AddNameValue("ENCODING", encoding.c_str());
    }
    auto path = File::formFileName(m_path, name, "tab");
    GDALDatasetPtr DS = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, optionsList);
    if(nullptr == DS) {
        errorMessage(_("Create of %s file failed. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    auto layer = DS->CreateLayer(name.c_str(), nullptr, wkbNone, optionsList);
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

    auto out = new MapInfoStoreTable(DS, layer, this, path, encoding);

    // Set description
    auto description = options.asString(DESCRIPTION_KEY);
    if(!description.empty()) {
        out->setProperty(DESCRIPTION_KEY, description, NG_ADDITIONS_KEY);
    }

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

    CPLStringList optionsList = options;
    auto defaultEncoding = property("ENCODING", "CP1251", NG_ADDITIONS_KEY);
    auto encoding = options.asString("ENCODING", defaultEncoding);
    if(!encoding.empty()) {
        optionsList.AddNameValue("ENCODING", encoding.c_str());
    }
    auto path = File::formFileName(m_path, name, "tab");
    GDALDatasetPtr DS = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, optionsList);
    if(!DS) {
        errorMessage(_("Create of %s file failed. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    auto layer = DS->CreateLayer(name.c_str(), spatialRef, type, optionsList);
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
    return out;
}

} // namespace ngs
