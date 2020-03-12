/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2020 NextGIS, <info@nextgis.com>
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
#include "table.h"

#include "api_priv.h"
#include "dataset.h"
#include "catalog/file.h"
#include "catalog/folder.h"
#include "ngstore/api.h"
#include "util/error.h"
#include "util/notify.h"

namespace ngs {

constexpr const char *FEATURE_SEPARATOR = "#";


//------------------------------------------------------------------------------
// FieldMapPtr
//------------------------------------------------------------------------------

FieldMapPtr::FieldMapPtr(const std::vector<Field> &src,
                         const std::vector<Field> &dst) :
    shared_ptr(static_cast<int*>(CPLMalloc(sizeof(int) * src.size())), CPLFree)
{
    matchFields(src, dst);
}

FieldMapPtr::FieldMapPtr(unsigned long size) :
    shared_ptr(static_cast<int*>(CPLMalloc(sizeof(int) * size)), CPLFree)
{

}

int &FieldMapPtr::operator[](int key)
{
    return get()[key];
}

const int &FieldMapPtr::operator[](int key) const
{
    return get()[key];
}

void FieldMapPtr::matchFields(const std::vector<Field> &src,
                   const std::vector<Field> &dst)
{
    for(size_t i = 0; i < src.size(); ++i) {
        bool matchFields = false;
        for (size_t j = 0; j < dst.size(); ++j) {
            if(compare(src[i].m_name, dst[j].m_name)) {
                get()[i] = static_cast<int>(j);
                matchFields = true;
                break;
            }
        }

        if(!matchFields) {
            get()[i] = -1;
        }
    }
}

//------------------------------------------------------------------------------
// FeaturePtr
//------------------------------------------------------------------------------

FeaturePtr::FeaturePtr(OGRFeature *feature, Table *table) :
    shared_ptr( feature, OGRFeature::DestroyFeature ),
    m_table(table)
{

}

FeaturePtr::FeaturePtr(OGRFeature *feature, const Table *table) :
    shared_ptr( feature, OGRFeature::DestroyFeature ),
    m_table(const_cast<Table*>(table))
{

}

FeaturePtr:: FeaturePtr() :
    shared_ptr( nullptr, OGRFeature::DestroyFeature ),
    m_table(nullptr)
{

}

FeaturePtr &FeaturePtr::operator=(OGRFeature *feature) {
    reset(feature);
    return *this;
}

ngs::FeaturePtr::operator OGRFeature *() const
{
     return get();
}

std::string FeaturePtr::dump(FeaturePtr::DumpOutputType type) const
{
    std::string out;
    if(nullptr != get()) {
        OGRGeometry *geom = get()->GetGeometryRef();
        char *wkt = nullptr;
        geom->exportToWkt(&wkt, wkbVariantIso);
        if(wkt) {
            out += std::string(wkt);
        }
        CPLFree(wkt);

        const char *style = nullptr;
        int styleFieldId = -1;
        if(type == DumpOutputType::HASH_STYLE) {
            styleFieldId = get()->GetFieldIndex("ogr_style");
        }

        for(int i = 0; i < get()->GetFieldCount(); ++i) {
            if(i == styleFieldId) {
                style = get()->GetFieldAsString(i);
            }
            else {
                out += "|" + std::string(get()->GetFieldAsString(i));
            }
        }
        // Skip style and native data in simple hash.

        if(type != DumpOutputType::HASH) {
            style = get()->GetStyleString();
            if(type != DumpOutputType::HASH_STYLE) {
                const char *native = get()->GetNativeData();
                if(native) {
                    out += "|" + std::string(native);
                }
            }
        }
        if(style) {
             out += "|" + std::string(style);
        }
    }
    switch (type) {
    case DumpOutputType::SIMPLE:
        return out;
    case DumpOutputType::HASH:
    case DumpOutputType::HASH_FULL:
    case DumpOutputType::HASH_STYLE:
        return md5(out);
    }
}

GIntBig FeaturePtr::addAttachment(const std::string &fileName,
                                  const std::string &description,
                                  const std::string &filePath,
                                  const Options &options, bool logEdits)
{
    if(nullptr == m_table) {
        outMessage(COD_INVALID, _("The feature detached from table"));
        return NOT_FOUND;
    }
    if(m_table->type() == CAT_QUERY_RESULT ||
       m_table->type() == CAT_QUERY_RESULT_FC) {
        outMessage(COD_INVALID, _("The feature from table that is result of query"));
        return NOT_FOUND;
    }

    return m_table->addAttachment(get()->GetFID(), fileName, description,
                                  filePath, options, logEdits);
}

GIntBig FeaturePtr::addAttachment(const FeaturePtr::AttachmentInfo &info,
                                  const Options &options, bool logEdits)
{
    return addAttachment(info.name, info.description, info.path, options, logEdits);
}

std::vector<FeaturePtr::AttachmentInfo> FeaturePtr::attachments() const
{
    if(nullptr == m_table) {
        outMessage(COD_INVALID, _("The feature detached from table"));
        return std::vector<FeaturePtr::AttachmentInfo>();
    }
    return m_table->attachments(get()->GetFID());
}

bool FeaturePtr::deleteAttachment(GIntBig aid, bool logEdits)
{
    if(nullptr == m_table) {
        outMessage(COD_INVALID, _("The feature detached from table"));
        return false;
    }
    return m_table->deleteAttachment(get()->GetFID(), aid, logEdits);
}

bool FeaturePtr::deleteAttachments(bool logEdits)
{
    if(nullptr == m_table) {
        outMessage(COD_INVALID, _("The feature detached from table"));
        return false;
    }
    return m_table->deleteAttachments(get()->GetFID(), logEdits);
}

bool FeaturePtr::updateAttachment(GIntBig aid, const std::string &fileName,
                                  const std::string &description, bool logEdits)
{
    if(nullptr == m_table) {
        outMessage(COD_INVALID, _("The feature detached from table"));
        return false;
    }
    return m_table->updateAttachment(get()->GetFID(), aid, fileName, description,
                                     logEdits);
}

Table *FeaturePtr::table() const
{
    return m_table;
}

void FeaturePtr::setTable(Table *table)
{
    m_table = table;
}

//------------------------------------------------------------------------------
// Table
//------------------------------------------------------------------------------

Table::Table(OGRLayer *layer,
             ObjectContainer * const parent,
             const enum ngsCatalogObjectType type,
             const std::string &name) :
    Object(parent, type, name, ""),
    m_layer(layer),
    m_attTable(nullptr),
    m_editHistoryTable(nullptr)
{
}

Table::~Table()
{
    if(m_type == CAT_QUERY_RESULT || m_type == CAT_QUERY_RESULT_FC) {
        Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
        if(nullptr != dataset) {
            dataset->releaseResultSet(this);
        }
    }
}

FeaturePtr Table::createFeature() const
{
    if(nullptr == m_layer) {
        return FeaturePtr();
    }

    OGRFeature *pFeature = OGRFeature::CreateFeature(m_layer->GetLayerDefn());
    if (nullptr == pFeature) {
        return FeaturePtr();
    }

    return FeaturePtr(pFeature, this);
}

FeaturePtr Table::getFeature(GIntBig id) const
{
    if(nullptr == m_layer) {
        return FeaturePtr();
    }
    MutexHolder holder(m_featureMutex);
    OGRFeature *pFeature = m_layer->GetFeature(id);
    if (nullptr == pFeature) {
        return FeaturePtr();
    }
    return FeaturePtr(pFeature, this);
}

bool Table::insertFeature(const FeaturePtr &feature, bool logEdits)
{
    if(nullptr == m_layer) {
        return false;
    }

    resetError();
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);

    // Lock all Dataset SQL queries here
    DatasetExecuteSQLLockHolder holder(dataset);
    if(m_layer->CreateFeature(feature) == OGRERR_NONE) {
        if(logEdits && saveEditHistory()) {
            FeaturePtr opFeature = logEditFeature(feature, FeaturePtr(),
                                                  CC_CREATE_FEATURE);
            logEditOperation(opFeature);
        }
        if(dataset && !dataset->isBatchOperation()) {
            Notify::instance().onNotify(fullName() + FEATURE_SEPARATOR +
                                        std::to_string(feature->GetFID()),
                                        ngsChangeCode::CC_CREATE_FEATURE);
        }
        onFeatureInserted(feature);
        return true;
    }

    return errorMessage(_("Failed to insert feature. %s"), CPLGetLastErrorMsg());
}

bool Table::updateFeature(const FeaturePtr &feature, bool logEdits)
{
    if(nullptr == m_layer) {
        return false;
    }

    resetError();
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);

    // Lock all Dataset SQL queries here
    DatasetExecuteSQLLockHolder holder(dataset);
    GIntBig id = feature->GetFID();
    FeaturePtr oldFeature = getFeature(id);
    if(m_layer->SetFeature(feature) == OGRERR_NONE) {
        if(logEdits && saveEditHistory()) {
            FeaturePtr opFeature = logEditFeature(feature, FeaturePtr(),
                                                  CC_CHANGE_FEATURE);
            logEditOperation(opFeature);
        }
        if(dataset && !dataset->isBatchOperation()) {
            Notify::instance().onNotify(fullName() + FEATURE_SEPARATOR +
                                        std::to_string(feature->GetFID()),
                                        ngsChangeCode::CC_CHANGE_FEATURE);
        }
        onFeatureUpdated(oldFeature, feature);
        return true;
    }

    return errorMessage(_("Failed to update feature. %s"), CPLGetLastErrorMsg());
}

bool Table::deleteFeature(GIntBig id, bool logEdits)
{
    if(nullptr == m_layer) {
        return false;
    }

    FeaturePtr delFeature = getFeature(id);
    FeaturePtr logFeature;
    if(saveEditHistory() && logEdits) {
        FeaturePtr feature = m_layer->GetFeature(id);
        logFeature = logEditFeature(feature, FeaturePtr(), CC_DELETE_FEATURE);
    }

    resetError();

    // Lock all Dataset SQL queries here
    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    if(m_layer->DeleteFeature(id) == OGRERR_NONE) {
        deleteAttachments(id, logEdits);

        if(logEdits && saveEditHistory()) {
            logEditOperation(logFeature);
        }
        Notify::instance().onNotify(fullName() + FEATURE_SEPARATOR +
                                    std::to_string(id),
                                    ngsChangeCode::CC_DELETE_FEATURE);
        onFeatureDeleted(delFeature);
        return true;
    }

    return errorMessage(_("Failed to delete feature. %s"), CPLGetLastErrorMsg());
}

bool Table::deleteFeatures(bool logEdits)
{
    if(nullptr == m_layer) {
        return false;
    }

    resetError();
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(dataset && dataset->deleteFeatures(storeName())) {
        if(logEdits && saveEditHistory()) {
            FeaturePtr logFeature = logEditFeature(FeaturePtr(), FeaturePtr(),
                                                   CC_DELETEALL_FEATURES);
            logEditOperation(logFeature);
        }
        Notify::instance().onNotify(fullName(),
                                    ngsChangeCode::CC_DELETEALL_FEATURES);
        dataset->destroyAttachmentsTable(storeName()); // Attachments table maybe not exists
        Folder::rmDir(getAttachmentsPath());

        featureCount(true);
        onFeaturesDeleted();
        return true;
    }

    return false;
}

GIntBig Table::featureCount(bool force) const
{
    if(nullptr == m_layer) {
        return 0;
    }

    MutexHolder holder(m_featureMutex);
    return m_layer->GetFeatureCount(force ? TRUE : FALSE);
}

void Table::reset() const
{
    if(nullptr != m_layer) {
        MutexHolder holder(m_featureMutex);
        m_layer->ResetReading();
    }
}

FeaturePtr Table::nextFeature() const
{
    if(nullptr == m_layer) {
        return FeaturePtr();
    }
    MutexHolder holder(m_featureMutex);
    return FeaturePtr(m_layer->GetNextFeature(), this);
}

int Table::copyRows(const TablePtr srcTable, const FieldMapPtr fieldMap,
                    const Progress& progress, const Options &options)
{
    if(!srcTable) {
        return outMessage(COD_COPY_FAILED, _("Source table is invalid"));
    }

    progress.onProgress(COD_IN_PROCESS, 0.0,
                       _("Start copy records from '%s' to '%s'"),
                       srcTable->name().c_str(), name().c_str());

    // Lock any SQL query in dataset
    DatasetBatchOperationHolder holder(dynamic_cast<Dataset*>(m_parent));

    GIntBig featureCount = srcTable->featureCount();
    double counter = 0;
    srcTable->reset();
    FeaturePtr feature;
    while((feature = srcTable->nextFeature())) {
        double complete = counter / featureCount;
        if(!progress.onProgress(COD_IN_PROCESS, complete,
                                _("Copy in process ..."))) {
            return  COD_CANCELED;
        }

        FeaturePtr dstFeature = createFeature();
        dstFeature->SetFieldsFrom(feature, fieldMap.get());

        if(!insertFeature(dstFeature, false)) {
            if(!progress.onProgress(COD_WARNING, complete,
                               _("Create feature failed. Source feature FID:" CPL_FRMT_GIB),
                               feature->GetFID ())) {
               return  COD_CANCELED;
            }
        }
        onRowCopied(feature, dstFeature, options);
        counter++;
    }

    progress.onProgress(COD_FINISHED, 1.0, _("Done. Copied %d rows"),
                       int(counter));

    return COD_SUCCESS;
}

bool Table::onRowsCopied(const TablePtr srcTable, const Progress &progress,
                         const Options &options)
{
    ngsUnused(srcTable);
    ngsUnused(progress);
    ngsUnused(options);
    return true;
}

std::string Table::fidColumn() const
{
    if(nullptr == m_layer) {
        return "";
    }
    return m_layer->GetFIDColumn();
}

bool Table::destroy()
{
    Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return errorMessage(_("Parent is not dataset"));
    }

    if(dataset->type() == CAT_CONTAINER_SIMPLE) {
        return dataset->destroy();
    }

    std::string name = m_name;
    std::string attPath = getAttachmentsPath();
    if(dataset->destroyTable(this)) {
        Folder::rmDir(attPath);
        return Object::destroy();
    }
    return false;
}

void Table::setAttributeFilter(const std::string &filter)
{
    if(nullptr != m_layer) {
        MutexHolder holder(m_featureMutex);
        if(filter.empty()) {
            m_layer->SetAttributeFilter(nullptr);
        }
        else {
            m_layer->SetAttributeFilter(filter.c_str());
        }
    }
}

OGRFeatureDefn *Table::definition() const
{
    if(nullptr == m_layer) {
        return nullptr;
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    return m_layer->GetLayerDefn();
}

OGRLayer *Table::attachmentsTable(bool init) const
{
    if(init && !initAttachmentsTable()) {
        return nullptr;
    }
    return m_attTable;
}

bool Table::initAttachmentsTable() const
{
    if(m_attTable) {
        return true;
    }

    Dataset* parentDS = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDS) {
        return false;
    }

    m_attTable = parentDS->getAttachmentsTable(storeName());
    if(nullptr == m_attTable) {
        m_attTable = parentDS->createAttachmentsTable(storeName());
    }

    return m_attTable != nullptr;
}

bool Table::initEditHistoryTable() const
{
    if(m_editHistoryTable) {
        return true;
    }

    Dataset* parentDS = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDS) {
        return false;
    }

    m_editHistoryTable = parentDS->getEditHistoryTable(storeName());
    if(nullptr == m_editHistoryTable) {
        m_editHistoryTable = parentDS->createEditHistoryTable(storeName());
    }

    return m_editHistoryTable != nullptr;
}

std::string Table::getAttachmentsPath(bool create) const
{
    auto dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return "";
    }

    auto dstRootPath = dataset->attachmentsFolderPath(create);
    return File::formFileName(dstRootPath, storeName(), "");
}

std::string Table::getAttachmentPath(GIntBig fid, GIntBig aid, bool createPath) const
{
    auto attachmentsPath = getAttachmentsPath(createPath);
    auto dstFeaturePath =
            File::formFileName(attachmentsPath, std::to_string(fid), "");
    if(createPath && !Folder::isExists(dstFeaturePath)) {
        Folder::mkDir(dstFeaturePath, true);
    }

    return File::formFileName(dstFeaturePath, std::to_string(aid), "");
}

void Table::fillFields() const
{
    m_fields.clear();
    if(nullptr != m_layer) {
        OGRFeatureDefn *defn = definition();

        if(nullptr == defn) {
            return;
        }

        Properties propertyList = properties(NG_ADDITIONS_KEY);

        for(int i = 0; i < defn->GetFieldCount(); ++i) {
            OGRFieldDefn *fieldDefn = defn->GetFieldDefn(i);
            Field fieldDesc;
            fieldDesc.m_type = fieldDefn->GetType();
            fieldDesc.m_name = fieldDefn->GetNameRef();

            fieldDesc.m_alias = propertyList.asString(CPLSPrintf("FIELD_%d_ALIAS", i));
            if(fieldDesc.m_alias.empty()) {
                fieldDesc.m_alias = fieldDesc.m_name;
            }

            fieldDesc.m_originalName = propertyList.asString(CPLSPrintf("FIELD_%d_NAME", i));
            if(fieldDesc.m_originalName.empty()) {
                fieldDesc.m_originalName = fieldDesc.m_name;
            }
            m_fields.push_back(fieldDesc);
        }

        // Fill metadata
        propertyList = properties(USER_KEY);
        for(auto it = propertyList.begin(); it != propertyList.end(); ++it) {
            if(m_layer->GetMetadataItem(it->first.c_str(), USER_KEY) == nullptr) {
                m_layer->SetMetadataItem(it->first.c_str(), it->second.c_str(),
                                         USER_KEY);
            }
        }
    }
}

GIntBig Table::addAttachment(GIntBig fid, const std::string &fileName,
                             const std::string &description,
                             const std::string &filePath,
                             const Options &options, bool logEdits)
{
    if(fid == NOT_FOUND) {
        return NOT_FOUND;
    }
    if(!initAttachmentsTable()) {
        return NOT_FOUND;
    }
    bool move = options.asBool("MOVE", false);

    FeaturePtr newAttachment = OGRFeature::CreateFeature(
                m_attTable->GetLayerDefn());

    newAttachment->SetField(ATTACH_FEATURE_ID_FIELD, fid);
    newAttachment->SetField(ATTACH_FILE_NAME_FIELD, fileName.c_str());
    newAttachment->SetField(ATTACH_DESCRIPTION_FIELD, description.c_str());

    if(m_attTable->CreateFeature(newAttachment) == OGRERR_NONE) {
        if(Folder::isExists(filePath)) {
            auto dstPath = getAttachmentPath(fid, newAttachment->GetFID(), true);
            if(move) {
                File::moveFile(filePath, dstPath);
            }
            else {
                File::copyFile(filePath, dstPath);
            }
        }

        if(logEdits && saveEditHistory() && nullptr != m_layer) {
            FeaturePtr feature = m_layer->GetFeature(fid);
            FeaturePtr logFeauture = logEditFeature(feature, newAttachment,
                                                    CC_CREATE_ATTACHMENT);

            logEditOperation(logFeauture);
        }
        return newAttachment->GetFID();
    }

    return NOT_FOUND;
}

bool Table::deleteAttachment(GIntBig fid, GIntBig aid, bool logEdits)
{
    ngsUnused(fid);
    if(!initAttachmentsTable()) {
        return false;
    }

    FeaturePtr attFeature = m_attTable->GetFeature(aid);
    bool result = m_attTable->DeleteFeature(aid) == OGRERR_NONE;
    if(result) {
        GIntBig fid = attFeature->GetFieldAsInteger64(ATTACH_FEATURE_ID_FIELD);
        auto attPath = getAttachmentPath(fid, aid, false);
        result = File::deleteFile(attPath);

        if(logEdits && saveEditHistory() && nullptr != m_layer) {
            FeaturePtr feature = m_layer->GetFeature(fid);
            FeaturePtr logFeauture = logEditFeature(feature, attFeature,
                                                    CC_DELETE_ATTACHMENT);
            logEditOperation(logFeauture);
        }
    }

    return result;
}

bool Table::deleteAttachments(GIntBig fid, bool logEdits)
{
    Dataset* dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    dataset->executeSQL(CPLSPrintf("DELETE FROM %s_%s WHERE %s = " CPL_FRMT_GIB,
                                   m_name.c_str(),
                                   Dataset::attachmentsFolderExtension().c_str(),
                                   ATTACH_FEATURE_ID_FIELD, fid));

    std::string attFeaturePath = File::formFileName(getAttachmentsPath(),
                                                    std::to_string(fid), "");
    Folder::rmDir(attFeaturePath);

    if(logEdits && saveEditHistory() && nullptr != m_layer) {
        FeaturePtr feature = m_layer->GetFeature(fid);
        FeaturePtr logFeauture = logEditFeature(feature, FeaturePtr(),
                                                CC_DELETEALL_ATTACHMENTS);
        logEditOperation(logFeauture);
    }
    return true;
}

bool Table::updateAttachment(GIntBig fid, GIntBig aid, const std::string &fileName,
                             const std::string &description, bool logEdits)
{
    ngsUnused(fid);
    if(!initAttachmentsTable()) {
        return false;
    }

    FeaturePtr attFeature = m_attTable->GetFeature(aid);
    if(!attFeature) {
        return false;
    }

    if(!fileName.empty()) {
        attFeature->SetField(ATTACH_FILE_NAME_FIELD, fileName.c_str());
    }
    if(!description.empty()) {
        attFeature->SetField(ATTACH_DESCRIPTION_FIELD, description.c_str());
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    if(m_attTable->SetFeature(attFeature) == OGRERR_NONE) {
        if(logEdits && saveEditHistory() && nullptr != m_layer) {
            GIntBig fid = attFeature->GetFieldAsInteger64(ATTACH_FEATURE_ID_FIELD);
            FeaturePtr feature = m_layer->GetFeature(fid);
            FeaturePtr logFeauture = logEditFeature(feature, attFeature,
                                                    CC_CHANGE_ATTACHMENT);
            logEditOperation(logFeauture);
        }
        return true;
    }

    return false;
}

std::vector<FeaturePtr::AttachmentInfo> Table::attachments(GIntBig fid) const
{
    std::vector<FeaturePtr::AttachmentInfo> out;

    if(!initAttachmentsTable()) {
        return out;
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    m_attTable->SetAttributeFilter(
                CPLSPrintf("%s = " CPL_FRMT_GIB, ATTACH_FEATURE_ID_FIELD, fid));
    //m_attTable->ResetReading();
    FeaturePtr attFeature;
    while((attFeature = m_attTable->GetNextFeature())) {
        FeaturePtr::AttachmentInfo info;
        info.name = attFeature->GetFieldAsString(ATTACH_FILE_NAME_FIELD);
        info.description = attFeature->GetFieldAsString(ATTACH_DESCRIPTION_FIELD);
        info.id = attFeature->GetFID();

        auto path = getAttachmentPath(fid, info.id, false);
        if(Folder::isExists(path)) {
            info.path = path;
            info.size = File::fileSize(info.path);
        }
        else {
            info.path = "";
            info.size = 0;
        }

        out.push_back(info);
    }

    m_attTable->SetAttributeFilter(nullptr);

    return out;
}

bool Table::canDestroy() const
{
    Dataset * const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    if(dataset->type() == CAT_CONTAINER_SIMPLE) {
        return dataset->canDestroy();
    }

    return !dataset->isReadOnly();
}

bool Table::checkSetProperty(const std::string &key, const std::string &value,
                             const std::string &domain)
{
    if(compare(key, LOG_EDIT_HISTORY_KEY) && compare(domain, NG_ADDITIONS_KEY)) {
        auto prevValue = saveEditHistory();
        auto currentValue = toBool(value);
        if(prevValue != currentValue && prevValue == true) {
            // Clear history table
            Dataset *parentDataset = dynamic_cast<Dataset*>(m_parent);
            if(nullptr != parentDataset) {
                parentDataset->clearEditHistoryTable(m_name);
            }
        }
    }
    return true;
}

bool Table::saveEditHistory()
{
    return toBool(property(LOG_EDIT_HISTORY_KEY, "OFF", NG_ADDITIONS_KEY));
}

std::string Table::fullPropertyDomain(const std::string &domain) const
{
    return m_name + "." + domain;
}

std::string Table::storeName() const
{
    return name();
}

void Table::onFeatureInserted(FeaturePtr feature)
{
    feature.setTable(this);
}

void Table::onFeatureUpdated(FeaturePtr oldFeature, FeaturePtr newFeature)
{
    ngsUnused(oldFeature);
    ngsUnused(newFeature);
}

void Table::onFeatureDeleted(FeaturePtr delFeature)
{
    ngsUnused(delFeature);
}

void Table::onFeaturesDeleted()
{

}

void Table::onRowCopied(FeaturePtr srcFeature, FeaturePtr dstFature,
                        const Options &options)
{
    ngsUnused(srcFeature);
    ngsUnused(dstFature);
    ngsUnused(options);
}

bool Table::setProperty(const std::string &key, const std::string &value,
                        const std::string &domain)
{
    bool result = false;
    if(nullptr != m_layer) {
        result = m_layer->SetMetadataItem(key.c_str(), value.c_str(),
                                          domain.c_str()) == CE_None;
    }

    if(!checkSetProperty(key, value, domain)) {
        return false;
    }

    Dataset *parentDataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDataset) {
        return result;
    }

    return parentDataset->setProperty(key, value, fullPropertyDomain(domain));
}

std::string Table::property(const std::string &key,
                            const std::string &defaultValue,
                            const std::string &domain) const
{
    Dataset* parentDataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr != m_layer) {
        DatasetExecuteSQLLockHolder holder(parentDataset);
        std::string internalProperty = fromCString(
                    m_layer->GetMetadataItem(key.c_str(), domain.c_str()));
        if(!internalProperty.empty()) {
            return internalProperty;
        }
    }

    if(nullptr == parentDataset) {
        return Object::property(key, defaultValue, domain);
    }
    return parentDataset->property(key, defaultValue, fullPropertyDomain(domain));

}

Properties Table::properties(const std::string &domain) const
{
    Properties out;
    Dataset *parentDataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr != m_layer) {
        DatasetExecuteSQLLockHolder holder(parentDataset);
        out = Properties(m_layer->GetMetadata(domain.c_str()));
    }

    if(nullptr == parentDataset) {
        return out;
    }
    Properties moreProperties = parentDataset->properties(fullPropertyDomain(domain));
    out.append(moreProperties);
    return out;
}

void Table::deleteProperties(const std::string &domain)
{
    Dataset *parentDataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr != m_layer) {
        DatasetExecuteSQLLockHolder holder(parentDataset);
        m_layer->SetMetadata(nullptr, domain.c_str());
    }

    if(nullptr == parentDataset) {
        return;
    }
    return parentDataset->deleteProperties(fullPropertyDomain(domain));
}

const std::vector<Field> &Table::fields() const
{
    if(m_fields.empty()) {
        fillFields();
    }
    return m_fields;
}

void Table::logEditOperation(const FeaturePtr &opFeature)
{
    if(!opFeature || nullptr == m_editHistoryTable) {
        return;
    }

    Dataset *parentDataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDataset) {
        return;
    }

    GIntBig fid = opFeature->GetFieldAsInteger64(FEATURE_ID_FIELD);
    GIntBig aid = opFeature->GetFieldAsInteger64(ATTACH_FEATURE_ID_FIELD);
    enum ngsChangeCode code =
            static_cast<enum ngsChangeCode>(opFeature->GetFieldAsInteger64(
                                                OPERATION_FIELD));

    DatasetExecuteSQLLockHolder holder(parentDataset);
    if(code == CC_DELETEALL_FEATURES) {
        parentDataset->clearEditHistoryTable(storeName());

        if(m_editHistoryTable->CreateFeature(opFeature) != OGRERR_NONE) {
            CPLDebug("ngstore", "Log operation %d failed", code);
        }

        return;
    }

    auto addsDS = parentDataset->m_addsDS;
    if(code == CC_DELETEALL_ATTACHMENTS) {
        if(fid == NOT_FOUND) {
            return;
        }
        addsDS->ExecuteSQL(CPLSPrintf("DELETE FROM %s WHERE %s = " CPL_FRMT_GIB " AND %s <> -1",
                                       parentDataset->historyTableName(m_name).c_str(),
                                       FEATURE_ID_FIELD, fid, ATTACH_FEATURE_ID_FIELD),
                           nullptr, nullptr);
        if(m_editHistoryTable->CreateFeature(opFeature) != OGRERR_NONE) {
            CPLDebug("ngstore", "Log operation %d failed", code);
        }

        return;
    }

    // Check delete all
    addsDS->ExecuteSQL(CPLSPrintf("DELETE FROM %s WHERE %s = %d",
                                   parentDataset->historyTableName(m_name).c_str(),
                                   OPERATION_FIELD, CC_DELETEALL_FEATURES),
                       nullptr, nullptr);

    if(code == CC_CREATE_ATTACHMENT || code == CC_CHANGE_ATTACHMENT) {
        if(fid == NOT_FOUND) {
            return;
        }
        addsDS->ExecuteSQL(CPLSPrintf("DELETE FROM %s WHERE %s = %d AND %s = " CPL_FRMT_GIB,
                                       parentDataset->historyTableName(m_name).c_str(),
                                       OPERATION_FIELD, CC_DELETEALL_ATTACHMENTS,
                                       FEATURE_ID_FIELD, fid),
                           nullptr, nullptr);
    }

    if(code == CC_CREATE_FEATURE || code == CC_CREATE_ATTACHMENT) {
        if(fid == NOT_FOUND) {
            return;
        }

        if(m_editHistoryTable->CreateFeature(opFeature) != OGRERR_NONE) {
            CPLDebug("ngstore", "Log operation %d failed", code);
        }

        return;
    }

    m_editHistoryTable->SetAttributeFilter(CPLSPrintf("%s = " CPL_FRMT_GIB,
                                                      FEATURE_ID_FIELD, fid));
    std::vector<FeaturePtr> features;
    FeaturePtr f;
    while((f = m_editHistoryTable->GetNextFeature())) {
        features.push_back(f);
    }
    m_editHistoryTable->SetAttributeFilter(nullptr);

    if(code == CC_DELETE_FEATURE) {
        if(fid == NOT_FOUND) {
            return;
        }

        if(!features.empty()) {
            addsDS->ExecuteSQL(CPLSPrintf("DELETE FROM %s WHERE %s = " CPL_FRMT_GIB,
                                       parentDataset->historyTableName(m_name).c_str(),
                                       FEATURE_ID_FIELD, fid),
                           nullptr, nullptr);
        }

        // If feature created and than deleted - nop
        for(auto feature : features) {
            enum ngsChangeCode testCode = static_cast<enum ngsChangeCode>(
                        feature->GetFieldAsInteger64(OPERATION_FIELD));

            if(testCode == CC_CREATE_FEATURE) {
                return;
            }
        }

        // Add new operation
        if(m_editHistoryTable->CreateFeature(opFeature) != OGRERR_NONE) {
            CPLDebug("ngstore", "Log operation %d failed", code);
        }

        return;
    }

    if(code == CC_DELETE_ATTACHMENT) {
        if(fid == NOT_FOUND || aid == NOT_FOUND) {
            return;
        }
        FeaturePtr attFeature;
        for(auto feature : features) {
            GIntBig testAid = feature->GetFieldAsInteger64(ATTACH_FEATURE_ID_FIELD);
            if(testAid == aid) {
                attFeature = feature;
                enum ngsChangeCode testCode = static_cast<enum ngsChangeCode>(
                            feature->GetFieldAsInteger64(OPERATION_FIELD));

                if(testCode == CC_CREATE_ATTACHMENT) {
                    if(m_editHistoryTable->DeleteFeature(feature->GetFID()) !=
                            OGRERR_NONE) {
                        CPLDebug("ngstore", "Failed delete log item");
                    }
                    return;
                }
                break;
            }
        }

        if(attFeature) {
            attFeature->SetField(OPERATION_FIELD, code);
            if(m_editHistoryTable->SetFeature(attFeature) != OGRERR_NONE) {
                CPLDebug("ngstore", "Failed update log item");
            }
            return;
        }

        if(m_editHistoryTable->CreateFeature(opFeature) != OGRERR_NONE) {
            CPLDebug("ngstore", "Log operation %d failed", code);
        }

        return;
    }

    if(code == CC_CHANGE_FEATURE) {
        if(fid == NOT_FOUND) {
            return;
        }
        // Check if feature deleted - skip
        // Check if feature added. If added - skip
        // Check if feature changed. If changed - skip
        if(!features.empty()) {
            return;
        }
        // Add new operation
        if(m_editHistoryTable->CreateFeature(opFeature) != OGRERR_NONE) {
            CPLDebug("ngstore", "Log operation %d failed", code);
        }

        return;
    }

    if(code == CC_CHANGE_ATTACHMENT) {
        if(fid == NOT_FOUND || aid == NOT_FOUND) {
            return;
        }
        // Check if attach deleted - skip
        // Check if attach added. If added - skip
        // Check if attach changed. If changed - skip
        for(auto feature : features) {
            GIntBig testAid = feature->GetFieldAsInteger64(ATTACH_FEATURE_ID_FIELD);
            if(testAid == aid) {
                return;
            }
        }

        // Add new operation
        if(m_editHistoryTable->CreateFeature(opFeature) != OGRERR_NONE) {
            CPLDebug("ngstore", "Log operation %d failed", code);
        }

        return;
    }

}

void Table::deleteEditOperation(const ngsEditOperation& op)
{
    Dataset *parentDataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDataset) {
        return;
    }

    DatasetExecuteSQLLockHolder holder(parentDataset);

    auto addsDS = parentDataset->m_addsDS;
    addsDS->ExecuteSQL(CPLSPrintf("DELETE FROM %s WHERE %s = " CPL_FRMT_GIB " AND %s = " CPL_FRMT_GIB /*" AND %s = %d"*/,
                                  parentDataset->historyTableName(m_name).c_str(),
                                  FEATURE_ID_FIELD, op.fid,
                                  ATTACH_FEATURE_ID_FIELD, op.aid/*,
                                  OPERATION_FIELD, op.code*/),
                           nullptr, nullptr);
}

std::vector<ngsEditOperation> Table::editOperations()
{
    std::vector<ngsEditOperation> out;
    if(!initEditHistoryTable()) {
        return out;
    }

    DatasetExecuteSQLLockHolder holder(dynamic_cast<Dataset*>(m_parent));
    FeaturePtr feature;
    m_editHistoryTable->ResetReading();
    while((feature = m_editHistoryTable->GetNextFeature())) {
        ngsEditOperation op;
        op.fid = feature->GetFieldAsInteger64(FEATURE_ID_FIELD);
        op.aid = feature->GetFieldAsInteger64(ATTACH_FEATURE_ID_FIELD);
        op.code = static_cast<enum ngsChangeCode>(feature->GetFieldAsInteger64(
                                                      OPERATION_FIELD));
        op.rid = NOT_FOUND;
        op.arid = NOT_FOUND;
        out.push_back(op);
    }
    return out;
}

bool Table::sync()
{
    if(nullptr != m_layer) {
        m_layer->ResetReading();
        return m_layer->SyncToDisk() == OGRERR_NONE;
    }
    return false;
}

/**
 * @brief Table::logEditFeature Create log edit feature. If both feature and attachFeature are null, no sence to log such operation.
 * @param feature Changed feature to add to the log. May be null.
 * @param attachFeature Changed attachment feature to add to the log. May be null.
 * @param code Operation code.
 * @return New log edit table feature.
 */
FeaturePtr Table::logEditFeature(FeaturePtr feature, FeaturePtr attachFeature,
                                      enum ngsChangeCode code)
{
    if(!saveEditHistory()) {
        return FeaturePtr();
    }

    if(!initEditHistoryTable()) {
        return FeaturePtr();
    }

    FeaturePtr newOp = OGRFeature::CreateFeature(m_editHistoryTable->GetLayerDefn());

    if(feature) {
        newOp->SetField(FEATURE_ID_FIELD, feature->GetFID());
    }
    else {
        newOp->SetField(FEATURE_ID_FIELD, NOT_FOUND);
    }

    if(attachFeature) {
        newOp->SetField(ATTACH_FEATURE_ID_FIELD, attachFeature->GetFID());
    }
    else {
        newOp->SetField(ATTACH_FEATURE_ID_FIELD, NOT_FOUND);
    }
    newOp->SetField(OPERATION_FIELD, code);

    return newOp;
}

OGRFieldType Table::fieldTypeFromName(const std::string &name)
{
    if(name.empty())
        return OFTMaxType;
    if(compare(name, "INTEGER"))
        return OFTInteger;
    if(compare(name, "INTEGER_LIST"))
        return OFTIntegerList;
    if(compare(name, "REAL"))
        return OFTReal;
    if(compare(name, "REAL_LIST"))
        return OFTRealList;
    if(compare(name, "STRING"))
        return OFTString;
    if(compare(name, "STRING_LIST"))
        return OFTStringList;
    if(compare(name, "BINARY"))
        return OFTBinary;
    if(compare(name, "DATE"))
        return OFTDate;
    if(compare(name, "TIME"))
        return OFTTime;
    if(compare(name, "DATE_TIME"))
        return OFTDateTime;
    if(compare(name, "INTEGER64"))
        return OFTInteger64;
    if(compare(name, "INTEGER64_LIST"))
        return OFTInteger64List;
    return OFTMaxType;
}

} // namespace ngs
