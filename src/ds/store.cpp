/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
#include "store.h"

#include "catalog/ngw.h"
#include "util.h"

namespace ngs {

//------------------------------------------------------------------------------
// StoreObject
//------------------------------------------------------------------------------

StoreObject::StoreObject(OGRLayer *layer) : m_storeIntLayer(layer)
{
}

FeaturePtr StoreObject::getFeatureByRemoteId(GIntBig rid) const
{
    const Table *table = dynamic_cast<const Table*>(this);
    if(nullptr == table) {
        return FeaturePtr();
    }

    Dataset *dataset = dynamic_cast<Dataset*>(table->parent());
    DatasetExecuteSQLLockHolder holder(dataset);
    auto attFilterStr = CPLSPrintf("%s = " CPL_FRMT_GIB, ngw::REMOTE_ID_KEY, rid);
    if(m_storeIntLayer->SetAttributeFilter(attFilterStr) != OGRERR_NONE) {
        return FeaturePtr();
    }
    FeaturePtr intFeature(m_storeIntLayer->GetNextFeature(), table);
    m_storeIntLayer->SetAttributeFilter(nullptr);

    return intFeature;
}

bool StoreObject::setAttachmentRemoteId(GIntBig aid, GIntBig rid)
{
    Table *table = dynamic_cast<Table*>(this);
    if(nullptr == table) {
        return false;
    }

    auto *attTable = table->attachmentsTable(true);
    if(nullptr == attTable) {
        return false;
    }

    Dataset *dataset = dynamic_cast<Dataset*>(table->parent());
    DatasetExecuteSQLLockHolder holder(dataset);
    FeaturePtr attFeature = attTable->GetFeature(aid);
    if(!attFeature) {
        return false;
    }

    attFeature->SetField(ngw::REMOTE_ID_KEY, rid);
    return attTable->SetFeature(attFeature) == OGRERR_NONE;
}


void StoreObject::setRemoteId(FeaturePtr feature, GIntBig rid)
{
    feature->SetField(ngw::REMOTE_ID_KEY, rid);
}

GIntBig StoreObject::getRemoteId(FeaturePtr feature)
{
    if(feature) {
        return feature->GetFieldAsInteger64(ngw::REMOTE_ID_KEY);
    }
    return NOT_FOUND;
}

GIntBig StoreObject::getRemoteId(GIntBig fid) const
{
    FeaturePtr feature = m_storeIntLayer->GetFeature(fid);
    if(nullptr == feature) {
        return NOT_FOUND;
    }
    return feature->GetFieldAsInteger64(ngw::REMOTE_ID_KEY);
}


std::vector<ngsEditOperation> StoreObject::fillEditOperations(
        OGRLayer *editHistoryTable, Dataset *dataset) const
{
    std::vector<ngsEditOperation> out;
    if(nullptr == editHistoryTable) {
        return out;
    }

    DatasetExecuteSQLLockHolder holder(dataset);
    FeaturePtr feature;
    editHistoryTable->ResetReading();
    while((feature = editHistoryTable->GetNextFeature())) {
        ngsEditOperation op;
        op.fid = feature->GetFieldAsInteger64(FEATURE_ID_FIELD);
        op.aid = feature->GetFieldAsInteger64(ATTACH_FEATURE_ID_FIELD);
        op.code = static_cast<enum ngsChangeCode>(feature->GetFieldAsInteger64(
                                                      OPERATION_FIELD));
        op.rid = StoreObject::getRemoteId(feature);
        op.arid = feature->GetFieldAsInteger64(ngw::ATTACHMENT_REMOTE_ID_KEY);
        if(op.arid == NOT_FOUND) {
            op.arid = getAttachmentRemoteId(op.aid);
        }
        out.push_back(op);
    }

    return out;
}

GIntBig StoreObject::getAttachmentRemoteId(GIntBig aid) const
{
    const Table *table = dynamic_cast<const Table*>(this);
    if(nullptr == table) {
        return NOT_FOUND;
    }

    auto *attTable = table->attachmentsTable();
    if(attTable == nullptr) {
        return NOT_FOUND;
    }

    FeaturePtr attFeature = attTable->GetFeature(aid);
    GIntBig rid = NOT_FOUND;
    if(attFeature) {
        rid = getRemoteId(attFeature);
    }
    return rid;
}

std::string StoreObject::downloadAttachment(GIntBig fid, GIntBig aid,
                                            const Progress &progress)
{
    return ngw::downloadAttachment(this, fid, aid, progress);
}

} // namespace ngs

