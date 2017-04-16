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

FeaturePtr::FeaturePtr(OGRFeature* feature) :
    shared_ptr( feature, OGRFeature::DestroyFeature )
{

}

FeaturePtr:: FeaturePtr() :
    shared_ptr( nullptr, OGRFeature::DestroyFeature )
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
    Object(parent, type, name, ""), m_layer(layer)
{
}

Table::~Table()
{
    if(m_type == ngsCatalogObjectType::CAT_QUERY_RESULT ||
       m_type == ngsCatalogObjectType::CAT_QUERY_RESULT_FC) {
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

    return FeaturePtr(pFeature);
}

FeaturePtr Table::getFeature(GIntBig id) const
{
    if(nullptr == m_layer)
        return FeaturePtr();

    OGRFeature* pFeature = m_layer->GetFeature(id);
    if (nullptr == pFeature)
        return FeaturePtr();

    return FeaturePtr(pFeature);
}

bool Table::insertFeature(const FeaturePtr &feature)
{
   if(nullptr == m_layer)
        return false;

    if(m_layer->CreateFeature(feature) == OGRERR_NONE) {
        Notify::instance().onNotify(getFullName(),
                                    ngsChangeCode::CC_CREATE_FEATURE);
        return true;
    }

    return false;
}

bool Table::updateFeature(const FeaturePtr &feature)
{
    if(nullptr == m_layer)
        return false;

    if(m_layer->SetFeature(feature) == OGRERR_NONE) {
        Notify::instance().onNotify(getFullName(),
                                    ngsChangeCode::CC_CHANGE_FEATURE);
        return true;
    }

    return false;
}

bool Table::deleteFeature(GIntBig id)
{
    if(nullptr == m_layer)
        return false;

    if(m_layer->DeleteFeature(id) == OGRERR_NONE) {
        Notify::instance().onNotify(getFullName(),
                                    ngsChangeCode::CC_DELETE_FEATURE);
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
    if(nullptr != m_layer)
        m_layer->ResetReading();
}

FeaturePtr Table::nextFeature() const
{
    if(nullptr == m_layer)
        return FeaturePtr();
    return m_layer->GetNextFeature();
}

int Table::copyRows(const TablePtr srcTable, const FieldMapPtr fieldMap,
                     const Progress& progress)
{
    if(!srcTable) {
        return errorMessage(ngsErrorCode::EC_COPY_FAILED, _("Source table is invalid"));
    }

    progress.onProgress(ngsErrorCode::EC_IN_PROCESS, 0.0,
                       _("Start copy records from '%s' to '%s'"),
                       srcTable->getName().c_str(), m_name.c_str());

    GIntBig featureCount = srcTable->featureCount();
    double counter = 0;
    srcTable->reset();
    FeaturePtr feature;
    while((feature = srcTable->nextFeature ())) {
        double complete = counter / featureCount;
        if(!progress.onProgress(ngsErrorCode::EC_IN_PROCESS, complete,
                           _("Copy in process ..."))) {
            return  ngsErrorCode::EC_CANCELED;
        }

        FeaturePtr dstFeature = createFeature();
        dstFeature->SetFieldsFrom(feature, fieldMap.get());

        if(!insertFeature(dstFeature)) {
            if(!progress.onProgress(ngsErrorCode::EC_WARNING, complete,
                               _("Create feature failed. Source feature FID:%lld"),
                               feature->GetFID ())) {
               return  ngsErrorCode::EC_CANCELED;
            }
        }
        counter++;
    }

    progress.onProgress(ngsErrorCode::EC_FINISHED, 1.0, _("Done. Copied %d rows"),
                       int(counter));

    return ngsErrorCode::EC_SUCCESS;
}

OGRFeatureDefn *Table::getDefinition() const
{
    if(nullptr == m_layer)
        return nullptr;
    return m_layer->GetLayerDefn();
}

const char* Table::getFIDColumn() const
{
    if(nullptr == m_layer)
        return "";
    return m_layer->GetFIDColumn();
}


bool Table::destroy()
{
    Dataset* const dataset = dynamic_cast<Dataset*>(m_parent);
    if(nullptr == dataset)
        return errorMessage(_("Parent is not dataset"));
    GDALDataset * const DS = dataset->getGDALDataset();
    if(nullptr == DS)
        return errorMessage(_("GDALDataset is null"));

    for(int i = 0; i < DS->GetLayerCount (); ++i){
        if(DS->GetLayer(i) == m_layer) {
            if(DS->DeleteLayer(i) == OGRERR_NONE) {
                m_layer = nullptr;
                if(m_parent)
                    m_parent->notifyChanges();
                Notify::instance().onNotify(getFullName(),
                                            ngsChangeCode::CC_DELETE_OBJECT);
                return true;
            }
        }
    }
    return false;

}

}
