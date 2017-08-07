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
#include "storefeatureclass.h"

#include "datastore.h"
#include "catalog/file.h"
#include "catalog/folder.h"

namespace ngs {

StoreFeatureClass::StoreFeatureClass(OGRLayer* layer,
                                     ObjectContainer* const parent,
                                     const CPLString& name) :
    FeatureClass(layer, parent, CAT_FC_GPKG, name)
{
    if(EQUAL(m_fields.back().m_name, REMOTE_ID_KEY)) {
        m_fields.pop_back();
    }
}

FeaturePtr StoreFeatureClass::getFeatureByRemoteId(GIntBig rid) const
{
    CPLMutexHolder holder(m_featureMutex);
    m_layer->SetAttributeFilter(CPLSPrintf("%s = " CPL_FRMT_GIB, REMOTE_ID_KEY, rid));
    m_layer->ResetReading();
    OGRFeature* pFeature = m_layer->GetNextFeature();
    FeaturePtr out;
    if (nullptr != pFeature) {
        out = FeaturePtr(pFeature, this);
    }
    m_layer->SetAttributeFilter(nullptr);
    return out;
}

bool StoreFeatureClass::setFeatureAttachmentRemoteId(GIntBig aid, GIntBig rid)
{
    if(!getAttachmentsTable()) {
        return false;
    }

    FeaturePtr attFeature = m_attTable->GetFeature(aid);
    if(!attFeature) {
        return false;
    }

    attFeature->SetField(REMOTE_ID_KEY, rid);
    return m_attTable->SetFeature(attFeature) == OGRERR_NONE;
}

void StoreFeatureClass::setRemoteId(FeaturePtr feature, GIntBig rid)
{
    feature->SetField(feature->GetFieldIndex(REMOTE_ID_KEY), rid);
}

GIntBig StoreFeatureClass::getRemoteId(FeaturePtr feature)
{
    return feature->GetFieldAsInteger64(feature->GetFieldIndex(REMOTE_ID_KEY));
}

void StoreFeatureClass::fillFields()
{
    Table::fillFields();
    // Hide remote id field from user
    if(EQUAL(m_fields.back().m_name, REMOTE_ID_KEY)) {
        m_fields.pop_back();
    }
}

std::vector<Table::AttachmentInfo> StoreFeatureClass::getAttachments(GIntBig fid)
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
        info.rid = attFeature->GetFieldAsInteger64(REMOTE_ID_KEY);

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

GIntBig StoreFeatureClass::addAttachment(GIntBig fid, const char* fileName,
                             const char* description, const char* filePath,
                             char** options)
{
    if(!getAttachmentsTable()) {
        return -1;
    }
    bool move = CPLFetchBool(options, "MOVE", false);
    GIntBig rid = atoll(CSLFetchNameValueDef(options, "RID", "-1"));

    FeaturePtr newAttachment = OGRFeature::CreateFeature(
                m_attTable->GetLayerDefn());

    newAttachment->SetField(ATTACH_FEATURE_ID, fid);
    newAttachment->SetField(ATTACH_FILE_NAME, fileName);
    newAttachment->SetField(ATTACH_DESCRIPTION, description);
    newAttachment->SetField(REMOTE_ID_KEY, rid);

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

} // namespace ngs

