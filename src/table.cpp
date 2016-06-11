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
#include "table.h"
#include "api.h"
#include "datastore.h"

using namespace ngs;

//------------------------------------------------------------------------------
// FeaturePtr
//------------------------------------------------------------------------------

FeaturePtr::FeaturePtr(OGRFeature* pFeature) :
    shared_ptr(pFeature, OGRFeature::DestroyFeature )
{

}

FeaturePtr:: FeaturePtr() :
    shared_ptr(nullptr, OGRFeature::DestroyFeature )
{

}

FeaturePtr& FeaturePtr::operator=(OGRFeature* pFeature) {
    reset(pFeature);
    return *this;
}

FeaturePtr::operator OGRFeature *() const
{
    return get();
}

//------------------------------------------------------------------------------
// Table
//------------------------------------------------------------------------------

Table::Table(OGRLayer* const layer, DataStore * datastore,
             const string& name, const string& alias) :
    Dataset(datastore, name, alias), m_layer(layer)
{
    m_type = TABLE;
}

FeaturePtr Table::createFeature() const
{
    if(m_deleted)
        return FeaturePtr();

    OGRFeature* pFeature = OGRFeature::CreateFeature( m_layer->GetLayerDefn() );
    if (nullptr == pFeature){
        return FeaturePtr();
    }

    return FeaturePtr(pFeature);
}

FeaturePtr Table::getFeature(GIntBig id) const
{
    if(m_deleted)
        return FeaturePtr();

    OGRFeature* pFeature = m_layer->GetFeature (id);
    if (nullptr == pFeature){
        return FeaturePtr();
    }

    return FeaturePtr(pFeature);
}

int Table::insertFeature(const FeaturePtr &feature)
{
    int nReturnCode = ngsErrorCodes::INSERT_FAILED;
    if(m_deleted)
        return nReturnCode;

    OGRErr eErr = m_layer->CreateFeature(feature);
    if(eErr == OGRERR_NONE)
        nReturnCode = ngsErrorCodes::SUCCESS;

    // notify datasource changed
    m_datastore->notifyDatasetCanged(DataStore::ADD_FEATURE, name(),
                                         feature->GetFID());

    return nReturnCode;
}

int Table::updateFeature(const FeaturePtr &feature)
{
    int nReturnCode = ngsErrorCodes::INSERT_FAILED;
    if(m_deleted)
        return nReturnCode;

    OGRErr eErr = m_layer->SetFeature(feature);
    if(eErr == OGRERR_NONE)
        nReturnCode = ngsErrorCodes::SUCCESS;

    // notify datasource changed
    m_datastore->notifyDatasetCanged(DataStore::CHANGE_FEATURE, name(),
                                         feature->GetFID());

    return nReturnCode;
}

int Table::deleteFeature(GIntBig id)
{
    if(m_deleted)
        return ngsErrorCodes::DELETE_FAILED;
    return m_layer->DeleteFeature (id) == OGRERR_NONE ? ngsErrorCodes::SUCCESS :
                                                        ngsErrorCodes::DELETE_FAILED;
}

GIntBig Table::featureCount(bool force) const
{
    if(m_deleted)
        return 0;

    return m_layer->GetFeatureCount (force ? TRUE : FALSE);
}

void Table::reset() const
{
    if(!m_deleted)
        m_layer->ResetReading ();
}

FeaturePtr Table::nextFeature() const
{
    if(m_deleted)
        return FeaturePtr();
    return m_layer->GetNextFeature ();
}
