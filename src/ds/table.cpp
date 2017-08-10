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
#include "table.h"

#include "api_priv.h"
#include "dataset.h"
#include "catalog/file.h"
#include "catalog/folder.h"
#include "ngstore/api.h"
#include "util/error.h"
#include "util/notify.h"

namespace ngs {

//------------------------------------------------------------------------------
// FieldMapPtr
//-------------------------------------------------

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

//------------------------------------------------------------------------------
// FeaturePtr
//------------------------------------------------------------------------------

FeaturePtr::FeaturePtr(OGRFeature* feature, Table* table) :
    shared_ptr( feature, OGRFeature::DestroyFeature ),
    m_table(table)
{

}

FeaturePtr::FeaturePtr(OGRFeature* feature, const Table* table) :
    shared_ptr( feature, OGRFeature::DestroyFeature ),
    m_table(const_cast<Table*>(table))
{

}

FeaturePtr:: FeaturePtr() :
    shared_ptr( nullptr, OGRFeature::DestroyFeature ),
    m_table(nullptr)
{

}

FeaturePtr& FeaturePtr::operator=(OGRFeature* feature) {
    reset(feature);
    return *this;
}

//------------------------------------------------------------------------------
// Table
//------------------------------------------------------------------------------

Table::Table(OGRLayer *layer,
             ObjectContainer * const parent,
             const enum ngsCatalogObjectType type,
             const CPLString &name) :
    Object(parent, type, name, ""),
    m_layer(layer),
    m_attTable(nullptr),
    m_featureMutex(CPLCreateMutex()),
    m_attMutex(CPLCreateMutex())
{
    CPLReleaseMutex(m_featureMutex);
    CPLReleaseMutex(m_attMutex);
    fillFields();
}

Table::~Table()
{
    CPLDestroyMutex(m_featureMutex);
    CPLDestroyMutex(m_attMutex);
    if(m_type == CAT_QUERY_RESULT || m_type == CAT_QUERY_RESULT_FC) {
        Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
        if(nullptr != dataset) {
            GDALDataset * const DS = dataset->getGDALDataset();
            if(nullptr != DS) {
                DS->ReleaseResultSet(m_layer);
            }
        }
    }
}

FeaturePtr Table::createFeature() const
{
    if(nullptr == m_layer)
        return FeaturePtr();

    OGRFeature* pFeature = OGRFeature::CreateFeature( m_layer->GetLayerDefn() );
    if (nullptr == pFeature)
        return FeaturePtr();

    return FeaturePtr(pFeature, this);
}

FeaturePtr Table::getFeature(GIntBig id) const
{
    if(nullptr == m_layer)
        return FeaturePtr();

    CPLMutexHolder holder(m_featureMutex);
    OGRFeature* pFeature = m_layer->GetFeature(id);
    if (nullptr == pFeature)
        return FeaturePtr();

    return FeaturePtr(pFeature, this);
}

bool Table::insertFeature(const FeaturePtr &feature)
{
   if(nullptr == m_layer)
        return false;

   CPLErrorReset();
    if(m_layer->CreateFeature(feature) == OGRERR_NONE) {
        Dataset* dataset = dynamic_cast<Dataset*>(m_parent);
        if(dataset && !dataset->isBatchOperation()) {
            Notify::instance().onNotify(fullName(),
                                    ngsChangeCode::CC_CREATE_FEATURE);
        }
        return true;
    }

    return errorMessage(COD_INSERT_FAILED, CPLGetLastErrorMsg());
}

bool Table::updateFeature(const FeaturePtr &feature)
{
    if(nullptr == m_layer)
        return false;

    CPLErrorReset();
    if(m_layer->SetFeature(feature) == OGRERR_NONE) {
        Notify::instance().onNotify(fullName(),
                                    ngsChangeCode::CC_CHANGE_FEATURE);
        return true;
    }

    return errorMessage(COD_UPDATE_FAILED, CPLGetLastErrorMsg());
}

bool Table::deleteFeature(GIntBig id)
{
    if(nullptr == m_layer)
        return false;

    CPLErrorReset();
    if(m_layer->DeleteFeature(id) == OGRERR_NONE) {
        deleteAttachments(id);
        Notify::instance().onNotify(fullName(),
                                    ngsChangeCode::CC_DELETE_FEATURE);
        return true;
    }

    return errorMessage(COD_DELETE_FAILED, CPLGetLastErrorMsg());
}

bool Table::deleteFeatures()
{
    if(nullptr == m_layer) {
        return false;
    }

    CPLErrorReset();
    Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    if(dataset->deleteFeatures(name())) {
        Notify::instance().onNotify(fullName(),
                                    ngsChangeCode::CC_DELETEALL_FEATURES);
        dataset->destroyAttachmentsTable(name()); // Attachments table maybe not exists
        Folder::rmDir(getAttachmentsPath());
        return true;
    }

    return false;
}

GIntBig Table::featureCount(bool force) const
{
    if(nullptr == m_layer)
        return 0;

    return m_layer->GetFeatureCount(force ? TRUE : FALSE);
}

void Table::reset() const
{
    CPLMutexHolder holder(m_featureMutex);
    if(nullptr != m_layer)
        m_layer->ResetReading();
}

FeaturePtr Table::nextFeature() const
{
    if(nullptr == m_layer)
        return FeaturePtr();
    CPLMutexHolder holder(m_featureMutex);
    return FeaturePtr(m_layer->GetNextFeature(), this);
}

int Table::copyRows(const TablePtr srcTable, const FieldMapPtr fieldMap,
                     const Progress& progress)
{
    if(!srcTable) {
        return errorMessage(COD_COPY_FAILED, _("Source table is invalid"));
    }

    progress.onProgress(COD_IN_PROCESS, 0.0,
                       _("Start copy records from '%s' to '%s'"),
                       srcTable->name().c_str(), m_name.c_str());

    GIntBig featureCount = srcTable->featureCount();
    double counter = 0;
    srcTable->reset();
    FeaturePtr feature;
    while((feature = srcTable->nextFeature ())) {
        double complete = counter / featureCount;
        if(!progress.onProgress(COD_IN_PROCESS, complete,
                           _("Copy in process ..."))) {
            return  COD_CANCELED;
        }

        FeaturePtr dstFeature = createFeature();
        dstFeature->SetFieldsFrom(feature, fieldMap.get());

        if(!insertFeature(dstFeature)) {
            if(!progress.onProgress(COD_WARNING, complete,
                               _("Create feature failed. Source feature FID:%lld"),
                               feature->GetFID ())) {
               return  COD_CANCELED;
            }
        }
        counter++;
    }

    progress.onProgress(COD_FINISHED, 1.0, _("Done. Copied %d rows"),
                       int(counter));

    return COD_SUCCESS;
}

const char* Table::fidColumn() const
{
    if(nullptr == m_layer)
        return "";
    return m_layer->GetFIDColumn();
}

char**Table::getMetadata(const char* domain) const
{
    if(nullptr == m_layer)
        return nullptr;
    return m_layer->GetMetadata(domain);
}

bool Table::destroy()
{
    CPLMutexHolder holder(m_featureMutex);
    Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return errorMessage(_("Parent is not dataset"));
    }

    CPLString fullNameStr = fullName();
    if(dataset->destroyTable(this)) {
        Notify::instance().onNotify(fullNameStr, ngsChangeCode::CC_DELETE_OBJECT);

        dataset->destroyAttachmentsTable(name()); // Attachments table maybe not exists
        Folder::rmDir(getAttachmentsPath());
        return true;
    }
    return false;
}

OGRFeatureDefn*Table::definition() const
{
    if(nullptr == m_layer)
        return nullptr;
    return m_layer->GetLayerDefn();
}

bool Table::getAttachmentsTable()
{
    if(m_attTable) {
        return true;
    }

    Dataset* parentDS = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == parentDS) {
        return false;
    }

    m_attTable = parentDS->getAttachmentsTable(name());
    if(nullptr == m_attTable) {
        m_attTable = parentDS->createAttachmentsTable(name());
    }

    return m_attTable != nullptr;
}

CPLString Table::getAttachmentsPath() const
{
    const char* dstRootPath = CPLResetExtension(
                m_parent->path(), Dataset::attachmentsFolderExtension());
    return CPLFormFilename(dstRootPath, name(), nullptr);
}

void Table::fillFields()
{
    m_fields.clear();
    if(nullptr != m_layer) {
        Dataset* parentDataset = dynamic_cast<Dataset*>(m_parent);
        OGRFeatureDefn* defn = m_layer->GetLayerDefn();
        if(nullptr == defn || nullptr == parentDataset) {
            return;
        }

        auto properties = parentDataset->getProperties(m_name);

        for(int i = 0; i < defn->GetFieldCount(); ++i) {
            OGRFieldDefn* fieldDefn = defn->GetFieldDefn(i);
            Field fieldDesc;
            fieldDesc.m_type = fieldDefn->GetType();
            fieldDesc.m_name = fieldDefn->GetNameRef();

            fieldDesc.m_alias = properties[CPLSPrintf("FIELD_%d_ALIAS", i)];
            fieldDesc.m_originalName = properties[CPLSPrintf("FIELD_%d_NAME", i)];

            m_fields.push_back(fieldDesc);
        }

        // Fill metadata
        for(auto it = properties.begin(); it != properties.end(); ++it) {
            if(EQUALN(it->first, "FIELD_", 6)) {
                continue;
            }
            m_layer->SetMetadataItem(it->first, it->second, KEY_USER);
        }
    }
}

GIntBig Table::addAttachment(GIntBig fid, const char* fileName,
                             const char* description, const char* filePath,
                             char** options)
{
    if(!getAttachmentsTable()) {
        return -1;
    }
    bool move = CPLFetchBool(options, "MOVE", false);

    FeaturePtr newAttachment = OGRFeature::CreateFeature(
                m_attTable->GetLayerDefn());

    newAttachment->SetField(ATTACH_FEATURE_ID, fid);
    newAttachment->SetField(ATTACH_FILE_NAME, fileName);
    newAttachment->SetField(ATTACH_DESCRIPTION, description);

    if(m_attTable->CreateFeature(newAttachment) == OGRERR_NONE) {
        CPLString dstTablePath = getAttachmentsPath();
        if(!Folder::isExists(dstTablePath)) {
            Folder::mkDir(dstTablePath);
        }
        CPLString dstFeaturePath = CPLFormFilename(dstTablePath,
                                                   CPLSPrintf(CPL_FRMT_GIB, fid),
                                                   nullptr);
        if(!Folder::isExists(dstFeaturePath)) {
            Folder::mkDir(dstFeaturePath);
        }

        CPLString dstPath = CPLFormFilename(dstFeaturePath,
                                            CPLSPrintf(CPL_FRMT_GIB,
                                                       newAttachment->GetFID()),
                                            nullptr);
        if(Folder::isExists(filePath)) {
            if(move) {
                File::moveFile(filePath, dstPath);
            }
            else {
                File::copyFile(filePath, dstPath);
            }
        }
        return newAttachment->GetFID();
    }

    return -1;
}

bool Table::deleteAttachment(GIntBig aid)
{
    if(!getAttachmentsTable()) {
        return false;
    }

    FeaturePtr attFeature = m_attTable->GetFeature(aid);
    bool result = m_attTable->DeleteFeature(aid) == OGRERR_NONE;
    if(result) {
        GIntBig fid = attFeature->GetFieldAsInteger64(ATTACH_FEATURE_ID);
        CPLString attFeaturePath = CPLFormFilename(getAttachmentsPath(),
                                                   CPLSPrintf(CPL_FRMT_GIB, fid),
                                                   nullptr);
        CPLString attPath = CPLFormFilename(attFeaturePath,
                                                   CPLSPrintf(CPL_FRMT_GIB, aid),
                                                   nullptr);

        result = File::deleteFile(attPath);
    }

    return result;
}

bool Table::deleteAttachments(GIntBig fid)
{
    Dataset* dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset) {
        return false;
    }

    dataset->executeSQL(CPLSPrintf("DELETE FROM %s_%s WHERE %s = " CPL_FRMT_GIB,
                                   m_name.c_str(), Dataset::attachmentsFolderExtension(),
                                   ATTACH_FEATURE_ID, fid));

    CPLString attFeaturePath = CPLFormFilename(getAttachmentsPath(),
                                               CPLSPrintf(CPL_FRMT_GIB, fid),
                                               nullptr);
    Folder::rmDir(attFeaturePath);
    return true;
}

bool Table::updateAttachment(GIntBig aid, const char* fileName,
                             const char* description)
{
    if(!getAttachmentsTable()) {
        return false;
    }

    FeaturePtr attFeature = m_attTable->GetFeature(aid);
    if(!attFeature)
        return false;

    if(fileName)
        attFeature->SetField(ATTACH_FILE_NAME, fileName);
    if(description)
        attFeature->SetField(ATTACH_DESCRIPTION, description);

    return m_attTable->SetFeature(attFeature) == OGRERR_NONE;
}

std::vector<Table::AttachmentInfo> Table::getAttachments(GIntBig fid)
{
    std::vector<AttachmentInfo> out;

    if(!getAttachmentsTable()) {
        return out;
    }

    CPLMutexHolder holder(m_attMutex);
    m_attTable->SetAttributeFilter(CPLSPrintf("%s = " CPL_FRMT_GIB,
                                              ATTACH_FEATURE_ID, fid));
    m_attTable->ResetReading();
    FeaturePtr attFeature;
    while((attFeature = m_attTable->GetNextFeature()) != nullptr) {
        AttachmentInfo info;
        info.name = attFeature->GetFieldAsString(ATTACH_FILE_NAME);
        info.description = attFeature->GetFieldAsString(ATTACH_DESCRIPTION);
        info.id = attFeature->GetFID();

        CPLString attFeaturePath = CPLFormFilename(getAttachmentsPath(),
                                                   CPLSPrintf(CPL_FRMT_GIB, fid),
                                                   nullptr);
        info.path = CPLFormFilename(attFeaturePath,
                                    CPLSPrintf(CPL_FRMT_GIB, info.id),
                                    nullptr);

        info.size = File::fileSize(info.path);

        out.push_back(info);
    }
    return out;
}


bool Table::canDestroy() const
{
    Dataset* const dataSet= dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataSet)
        return false;
    return !dataSet->isReadOnly();
}

} // namespace ngs
