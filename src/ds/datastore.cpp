/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
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

#include "datastore.h"

// gdal
#include "cpl_conv.h"
#include "cpl_vsi.h"

// stl
#include <iostream>

#include "api_priv.h"
#include "geometry.h"
#include "storefeatureclass.h"

#include "catalog/catalog.h"
#include "catalog/file.h"
#include "catalog/folder.h"

#include "ngstore/catalog/filter.h"
#include "ngstore/codes.h"
#include "ngstore/util/constants.h"
#include "ngstore/version.h"

#include "util.h"
#include "util/notify.h"
#include "util/error.h"
#include "util/stringutil.h"


namespace ngs {

constexpr const char *STORE_EXT = "ngst"; // NextGIS Store
constexpr int STORE_EXT_LEN = length(STORE_EXT);


//------------------------------------------------------------------------------
// DataStore
//------------------------------------------------------------------------------

DataStore::DataStore(ObjectContainer * const parent,
                     const std::string &name,
                     const std::string &path) :
    Dataset(parent, CAT_CONTAINER_NGS, name, path), 
    SpatialDataset(),
    m_disableJournalCounter(0)
{
    m_spatialReference = SpatialReferencePtr::importFromEPSG(DEFAULT_EPSG);
}

bool DataStore::isNameValid(const std::string &name) const
{
    if(comparePart(name, STORE_EXT, STORE_EXT_LEN)) {
        return false;
    }
    return Dataset::isNameValid(name);
}

std::string DataStore::normalizeFieldName(const std::string &name) const
{
    if(compare(ngw::REMOTE_ID_KEY, name)) {
        std::string out = name + "_";
        return out;
    }
    if(compare("fid", name) || compare("geom", name)) {
        std::string out = name + "_";
        return out;
    }
    return Dataset::normalizeFieldName(name);
}

void DataStore::fillFeatureClasses() const
{
    for(int i = 0; i < m_DS->GetLayerCount(); ++i) {
        OGRLayer *layer = m_DS->GetLayer(i);
        if(nullptr != layer) {
            OGRwkbGeometryType geometryType = layer->GetGeomType();
            if(skipFillFeatureClass(layer)) {
                continue;
            }

            const char *layerName = layer->GetName();
            DataStore *parent = const_cast<DataStore*>(this);
            if(geometryType == wkbNone) {
                m_children.push_back(ObjectPtr(new StoreTable(layer, parent, layerName)));
            }
            else {
                m_children.push_back(ObjectPtr(new StoreFeatureClass(layer, parent, layerName)));
            }
        }
    }
}

bool DataStore::create(const std::string &path)
{
    resetError();
    if(path.empty()) {
        return errorMessage(_("The path is empty"));
    }

    GDALDriver *driver = Filter::getGDALDriver(CAT_CONTAINER_NGS);
    if(driver == nullptr) {
        return errorMessage(_("Driver is not present"));
    }

    GDALDataset *DS = driver->Create(path.c_str(), 0, 0, 0, GDT_Unknown, nullptr);
    if(DS == nullptr) {
        return errorMessage(_("Failed to create datastore. %s"), CPLGetLastErrorMsg());
    }

    createMetadataTable(DS);

    GDALClose(DS);
    return true;
}

std::string DataStore::extension()
{
    return STORE_EXT;
}

bool DataStore::open(unsigned int openFlags, const Options &options)
{
    if(isOpened()) {
        return true;
    }

    if(!Dataset::open(openFlags, options)) {
        return false;
    }

    executeSQL("PRAGMA journal_mode=WAL", "SQLite");
    executeSQL("PRAGMA busy_timeout = 120000", "SQLite"); // 2m timeout

    resetError();

    int version = std::stoi(property(NGS_VERSION_KEY, "0"));

    if(version < NGS_VERSION_NUM && !upgrade(version)) {
        return errorMessage(_("Upgrade storage failed"));
    }

    return true;
}

FeatureClass *DataStore::createFeatureClass(const std::string &name,
                                            enum ngsCatalogObjectType objectType,
                                            OGRFeatureDefn * const definition,
                                            SpatialReferencePtr spatialRef,
                                            OGRwkbGeometryType type,
                                            const Options &options,
                                            const Progress& progress)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened"));
        return nullptr;
    }

    ngsUnused(objectType);

    resetError();

    MutexHolder holder(m_executeSQLMutex);

    OGRLayer *layer = m_DS->CreateLayer(name.c_str(), spatialRef, type,
                                        options.asCPLStringList());

    if(layer == nullptr) {
        errorMessage(_("Failed to create feature class. %s"), CPLGetLastErrorMsg());
        return nullptr;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        std::string newFieldName;
        if(definition->GetFieldCount() - 1 == i) {
            newFieldName = srcField->GetNameRef();
        }
        else {
            newFieldName = normalizeFieldName(srcField->GetNameRef());
            if(!compare(newFieldName, srcField->GetNameRef())) {
                progress.onProgress(COD_WARNING, 0.0,
                                    _("Field %s of source table was renamed to %s in destination tables"),
                                    srcField->GetNameRef(), newFieldName.c_str());
            }
        }

        dstField.SetName(newFieldName.c_str());
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(_("Failed to create field %s. %s"), newFieldName.c_str(), CPLGetLastErrorMsg());
            return nullptr;
        }
    }

    FeatureClass *out = new StoreFeatureClass(layer, this, name);

    if(options.asBool("CREATE_OVERVIEWS", false) &&
            !options.asString("ZOOM_LEVELS", "").empty()) {
        out->createOverviews(progress, options);
    }

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->fullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

Table *DataStore::createTable(const std::string &name,
                              enum ngsCatalogObjectType objectType,
                              OGRFeatureDefn* const definition,
                              const Options& options, const Progress& progress)
{
    if(nullptr == m_DS) {
        errorMessage(_("Not opened"));
        return nullptr;
    }

    resetError();

    ngsUnused(objectType);

    MutexHolder holder(m_executeSQLMutex);

    OGRLayer *layer = m_DS->CreateLayer(name.c_str(), nullptr, wkbNone,
                                        options.asCPLStringList());

    if(layer == nullptr) {
        errorMessage(_("Failed to create table %s. %s"), name.c_str(), CPLGetLastErrorMsg());
        return nullptr;
    }

    for (int i = 0; i < definition->GetFieldCount(); ++i) { // Don't check remote id field
        OGRFieldDefn *srcField = definition->GetFieldDefn(i);
        OGRFieldDefn dstField(srcField);

        std::string newFieldName;
        if(definition->GetFieldCount() - 1 == i) {
            newFieldName = srcField->GetNameRef();
        }
        else {
            newFieldName = normalizeFieldName(srcField->GetNameRef());
            if(!compare(newFieldName, srcField->GetNameRef())) {
                progress.onProgress(COD_WARNING, 0.0,
                                    _("Field %s of source table was renamed to %s in destination tables"),
                                    srcField->GetNameRef(), newFieldName.c_str());
            }
        }

        dstField.SetName(newFieldName.c_str());
        if (layer->CreateField(&dstField) != OGRERR_NONE) {
            errorMessage(_("Failed to create field %s. %s"), newFieldName.c_str(), CPLGetLastErrorMsg());
            return nullptr;
        }
    }

    Table *out = new StoreTable(layer, this, name);

    if(m_parent) {
        m_parent->notifyChanges();
    }

    Notify::instance().onNotify(out->fullName(), ngsChangeCode::CC_CREATE_OBJECT);

    return out;
}

bool DataStore::setProperty(const std::string &key, const std::string &value,
                            const std::string &domain)
{
    return m_DS->SetMetadataItem(key.c_str(), value.c_str(), domain.c_str()) ==
            OGRERR_NONE;
}

std::string DataStore::property(const std::string &key,
                                const std::string &defaultValue,
                                const std::string &domain) const
{
    const char *out = m_DS->GetMetadataItem(key.c_str(), domain.c_str());
    return nullptr == out ? defaultValue : std::string(out);
}

Properties DataStore::properties(const std::string &domain) const
{
    if(nullptr == m_DS) {
        return Properties();
    }

    return Properties(m_DS->GetMetadata(domain.c_str()));
}

bool DataStore::canCreate(const enum ngsCatalogObjectType type) const
{
    if(!isOpened() || isReadOnly()) {
        return false;
    }
    return type == CAT_FC_GPKG || type == CAT_TABLE_GPKG;
}

ObjectPtr DataStore::create(const enum ngsCatalogObjectType type,
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
    CREATE_FEATURE_DEFN_RESULT featureDefnStruct = createFeatureDefinition(name, options);

    // Add remote id field
    OGRFieldDefn ridField(ngw::REMOTE_ID_KEY, OFTInteger64);
    ridField.SetDefault(CPLSPrintf(CPL_FRMT_GIB, ngw::INIT_RID_COUNTER));
    featureDefnStruct.defn->AddFieldDefn(&ridField);

    ObjectPtr object;
    if(type == CAT_FC_GPKG) {
        OGRwkbGeometryType geomType = FeatureClass::geometryTypeFromName(
                    options.asString("GEOMETRY_TYPE", ""));
        if(wkbUnknown == geomType) {
            errorMessage(_("Unsupported geometry type"));
            return ObjectPtr();
        }

        object = ObjectPtr(createFeatureClass(newName, CAT_FC_GPKG,
                                              featureDefnStruct.defn,
                                              m_spatialReference, geomType,
                                              options));
    }
    else if(type == CAT_TABLE_GPKG) {
        object = ObjectPtr(createTable(newName, CAT_TABLE_GPKG,
                                       featureDefnStruct.defn,
                                       options));
    }

    setMetadata(object, featureDefnStruct.fields, options);

    if(object && m_childrenLoaded) {
        m_children.push_back(object);
    }
    return object;
}

bool DataStore::upgrade(int oldVersion)
{
    executeSQL("VACUUM", "SQLite");
    ngsUnused(oldVersion);
    // no structure changes for version 1
    return true;
}

void DataStore::enableJournal(bool enable)
{
    if(enable) {
        m_disableJournalCounter--;
        if(m_disableJournalCounter == 0) {
            // executeSQL("PRAGMA synchronous = FULL", "SQLite");
            executeSQL("PRAGMA journal_mode = WAL", "SQLite");
            //executeSQL("PRAGMA count_changes=ON", "SQLite"); // This pragma is deprecated
        }
    }
    else {
        CPLAssert(m_disableJournalCounter < 255); // only 255 layers can simultanious load geodata
        m_disableJournalCounter++;
        if(m_disableJournalCounter == 1) {
            // executeSQL("PRAGMA synchronous = OFF", "SQLite");
            executeSQL("PRAGMA journal_mode = OFF", "SQLite");
            //executeSQL("PRAGMA count_changes=OFF", "SQLite"); // This pragma is deprecated
            // executeSQL ("PRAGMA locking_mode=EXCLUSIVE", "SQLite");
            // executeSQL ("PRAGMA cache_size=15000", "SQLite");
        }
    }
}

OGRLayer *DataStore::createAttachmentsTable(const std::string &name)
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

OGRLayer *DataStore::createEditHistoryTable(const std::string &name)
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

bool DataStore::isBatchOperation() const
{
    return m_disableJournalCounter > 0;
}

bool DataStore::hasTracksTable() const
{
    if(nullptr == m_DS) {
        return false;
    }
    return m_DS->GetLayerByName(TRACKS_TABLE) != nullptr;
}

bool DataStore::createTracksTable()
{
    if(nullptr == m_DS) {
        return false;
    }

    CPLStringList options;
    options.AddString("GEOMETRY_NULLABLE=NO");
    options.AddString("SPATIAL_INDEX=NO");

    MutexHolder holder(m_executeSQLMutex);

    m_DS->StartTransaction();

    // GPX_ELE_AS_25D
    OGRLayer *layer = m_DS->CreateLayer(TRACKS_POINTS_TABLE, m_spatialReference, 
        wkbPoint, options);

    if(layer == nullptr) {
        m_DS->RollbackTransaction();
        return errorMessage(CPLGetLastErrorMsg());
    }

    // Add fields

    OGRFieldDefn trackFIDField("track_fid", OFTInteger);
    trackFIDField.SetNullable(FALSE);
    if(layer->CreateField(&trackFIDField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn trackSeqIDField("track_seg_id", OFTInteger);
    trackSeqIDField.SetNullable(FALSE);
    if(layer->CreateField(&trackSeqIDField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn trackSeqPtIDField("track_seg_point_id", OFTInteger);
    trackSeqPtIDField.SetNullable(FALSE);
    if(layer->CreateField(&trackSeqPtIDField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn nameField("track_name", OFTString);
    nameField.SetWidth(127);
    if(layer->CreateField(&nameField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn eleField("ele", OFTReal);
    eleField.SetDefault("0.0");
    if(layer->CreateField(&eleField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn timeField("time", OFTDateTime);
    timeField.SetNullable(FALSE);
    if(layer->CreateField(&timeField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

//    OGRFieldDefn magvarField("magvar", OFTReal);
//    magvarField.SetDefault("0.0");
//    if(layer->CreateField(&magvarField) != OGRERR_NONE) {
//        return false;
//    }

//    OGRFieldDefn geoidheightField("geoidheight", OFTReal);
//    geoidheightField.SetDefault("0.0");
//    if(layer->CreateField(&geoidheightField) != OGRERR_NONE) {
//        return false;
//    }

//    OGRFieldDefn nameField2("name", OFTString );
//    if(layer->CreateField(&nameField2) != OGRERR_NONE) {
//        return false;
//    }

//    OGRFieldDefn cmtField("cmt", OFTString );
//    if(layer->CreateField(&cmtField) != OGRERR_NONE) {
//        return false;
//    }

    OGRFieldDefn descField("desc", OFTString);
    descField.SetDefault(NGS_USERAGENT);
    if(layer->CreateField(&descField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn srcField("src", OFTString);
    const char *appName = CPLGetConfigOption("APP_NAME", "ngstore");
    srcField.SetDefault(appName);
    if(layer->CreateField(&srcField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

//    for(int i = 1;i <= 2; ++i) {
//        char szFieldName[32];
//        snprintf(szFieldName, sizeof(szFieldName), "link%d_href", i);
//        OGRFieldDefn oFieldLinkHref( szFieldName, OFTString );
//        if(layer->CreateField(&oFieldLinkHref) != OGRERR_NONE) {
//            return false;
//        }

//        snprintf(szFieldName, sizeof(szFieldName), "link%d_text", i);
//        OGRFieldDefn oFieldLinkText( szFieldName, OFTString );
//        if(layer->CreateField(&oFieldLinkText) != OGRERR_NONE) {
//            return false;
//        }

//        snprintf(szFieldName, sizeof(szFieldName), "link%d_type", i);
//        OGRFieldDefn oFieldLinkType( szFieldName, OFTString );
//        if(layer->CreateField(&oFieldLinkType) != OGRERR_NONE) {
//            return false;
//        }
//    }

//    OGRFieldDefn symField("sym", OFTString );
//    if(layer->CreateField(&symField) != OGRERR_NONE) {
//        return false;
//    }

//    OGRFieldDefn typeField("type", OFTString );
//    if(layer->CreateField(&typeField) != OGRERR_NONE) {
//        return false;
//    }

    /* Accuracy info */

    // A 2D fix gives only longitude and latitude. It needs a minimum of 3 satellites.
    // A 3D fix gives full longitude latitude + altitude position. It needs a minimum of 4 satellites.
    OGRFieldDefn fixField("fix", OFTString);
    fixField.SetWidth(3);
    if(layer->CreateField(&fixField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn satField("sat", OFTInteger);
    satField.SetDefault("0");
    if(layer->CreateField(&satField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

//    OGRFieldDefn hdopField("hdop", OFTReal );
//    hdopField.SetDefault("0.0");
//    if(layer->CreateField(&hdopField) != OGRERR_NONE) {
//        return false;
//    }

//    OGRFieldDefn vdopField("vdop", OFTReal );
//    vdopField.SetDefault("0.0");
//    if(layer->CreateField(&vdopField) != OGRERR_NONE) {
//        return false;
//    }

    OGRFieldDefn pdopField("pdop", OFTReal);
    pdopField.SetDefault("0.0");
    if(layer->CreateField(&pdopField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

//    OGRFieldDefn ageofgpsdataField("ageofdgpsdata", OFTReal );
//    if(layer->CreateField(&ageofgpsdataField) != OGRERR_NONE) {
//        return false;
//    }

//    OGRFieldDefn dgpsidField("dgpsid", OFTInteger );
//    if(layer->CreateField(&dgpsidField) != OGRERR_NONE) {
//        return false;
//    }

    // Addtional fields
    OGRFieldDefn courseField("course", OFTReal);
    courseField.SetDefault("0.0");
    if(layer->CreateField(&courseField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn speedField("speed", OFTReal);
    speedField.SetDefault("0.0");
    if(layer->CreateField(&speedField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn timeStampField("time_stamp", OFTDateTime);
    timeStampField.SetDefault("CURRENT_TIMESTAMP");
    if(layer->CreateField(&timeStampField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn syncedField("synced", OFTInteger);
    syncedField.SetDefault("0");
    if(layer->CreateField(&syncedField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    layer = m_DS->CreateLayer(TRACKS_TABLE, m_spatialReference, wkbMultiLineString);
    if(layer == nullptr) {
        m_DS->RollbackTransaction();
        return errorMessage(CPLGetLastErrorMsg());
    }

    if(layer->CreateField(&trackFIDField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    if(layer->CreateField(&nameField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn startTimeField("start_time", OFTDateTime);
    startTimeField.SetNullable(FALSE);
    if(layer->CreateField(&startTimeField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn stopTimeField("stop_time", OFTDateTime);
    stopTimeField.SetNullable(FALSE);
    if(layer->CreateField(&stopTimeField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    OGRFieldDefn pointsCountField("points_count", OFTInteger64);
    if(layer->CreateField(&pointsCountField) != OGRERR_NONE) {
        m_DS->RollbackTransaction();
        return false;
    }

    return m_DS->CommitTransaction() == OGRERR_NONE;
}

ObjectPtr DataStore::getTracksTable()
{
    if(m_tracksTable) {
        return m_tracksTable;
    }

    if(!hasTracksTable()) {
        if(!createTracksTable()) {
            return ObjectPtr();
        }
    }

    m_tracksTable = ObjectPtr(new TracksTable(m_DS->GetLayerByName(TRACKS_TABLE),
            m_DS->GetLayerByName(TRACKS_POINTS_TABLE), this));
    return m_tracksTable;
}

bool DataStore::destroyTracksTable()
{
    OGRLayer *layer = m_DS->GetLayerByName(TRACKS_POINTS_TABLE);
    if(!layer) {
        return false;
    }
    return destroyTable(m_DS, layer);
}

} // namespace ngs
