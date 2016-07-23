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
#include "api.h"
#include "datastore.h"

using namespace ngs;

//------------------------------------------------------------------------------
// FieldMapPtr
//-------------------------------------------------

FieldMapPtr::FieldMapPtr(unsigned long size) : shared_ptr(static_cast<int*>(CPLMalloc(
                                                sizeof(int) * size)), CPLFree)
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

Table::Table(OGRLayer * const layer) : Dataset(), m_layer(layer)
{
    m_type = TABLE;
    m_opened = true;
    if(layer)
        m_name = layer->GetName ();
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
   if(m_deleted)
        return ngsErrorCodes::INSERT_FAILED;

    int nReturnCode = m_layer->CreateFeature(feature) == OGRERR_NONE ?
                ngsErrorCodes::SUCCESS : ngsErrorCodes::INSERT_FAILED;

    // notify datasource changed
    if(nReturnCode == ngsErrorCodes::SUCCESS)
        notifyDatasetChanged(Dataset::ADD_FEATURE, name(), feature->GetFID());

    return nReturnCode;
}

int Table::updateFeature(const FeaturePtr &feature)
{
    if(m_deleted)
        return ngsErrorCodes::UPDATE_FAILED;

    int nReturnCode = m_layer->SetFeature(feature) == OGRERR_NONE ?
                ngsErrorCodes::SUCCESS : ngsErrorCodes::UPDATE_FAILED;

    // notify datasource changed
    if(nReturnCode == ngsErrorCodes::SUCCESS)
        notifyDatasetChanged(Dataset::CHANGE_FEATURE, name(), feature->GetFID());

    return nReturnCode;
}

int Table::deleteFeature(GIntBig id)
{
    if(m_deleted)
        return ngsErrorCodes::DELETE_FAILED;

    int nReturnCode = m_layer->DeleteFeature (id) == OGRERR_NONE ?
                ngsErrorCodes::SUCCESS : ngsErrorCodes::DELETE_FAILED;
    // notify datasource changed
    if(nReturnCode == ngsErrorCodes::SUCCESS)
        notifyDatasetChanged(Dataset::DELETE_FEATURE, name(), id);

    return nReturnCode;
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

ResultSetPtr Table::executeSQL(const CPLString &statement,
                               const CPLString &dialect) const
{
    return ResultSetPtr(m_DS->ExecuteSQL ( statement, nullptr, dialect ));
}

int Table::destroy(ngsProgressFunc progressFunc, void *progressArguments)
{
    if(progressFunc)
        progressFunc(0, CPLSPrintf ("Deleting %s", name().c_str ()),
                     progressArguments);
    for(int i = 0; i < m_DS->GetLayerCount (); ++i){
        if(m_DS->GetLayer (i) == m_layer) {
            m_DS->DeleteLayer (i);
            m_deleted = true;
            if(progressFunc)
                progressFunc(1, CPLSPrintf ("Deleted %s", name().c_str ()),
                             progressArguments);

            return ngsErrorCodes::SUCCESS;
        }
    }
    return ngsErrorCodes::DELETE_FAILED;
}

int Table::copyRows(const Table *pSrcTable, const FieldMapPtr fieldMap,
                    ngsProgressFunc progressFunc, void *progressArguments)
{
    if(progressFunc)
        progressFunc(0, CPLSPrintf ("Start copy records from '%s' to '%s'",
                                    pSrcTable->name ().c_str (), name().c_str ()),
                     progressArguments);

    GIntBig featureCount = pSrcTable->featureCount();
    double counter = 0;
    pSrcTable->reset ();
    FeaturePtr feature;
    while((feature = pSrcTable->nextFeature ()) != nullptr) {
        if(progressFunc)
            progressFunc(counter / featureCount, "copying...", progressArguments);

        FeaturePtr dstFeature = createFeature ();
        dstFeature->SetFieldsFrom (feature, fieldMap.get());

        if(insertFeature(dstFeature) != ngsErrorCodes::SUCCESS) {
            CPLError(CE_Warning, CPLE_AppDefined, "Create feature failed. "
                     "Source feature FID:%lld", feature->GetFID ());
        }
        counter++;
    }

    return ngsErrorCodes::SUCCESS;
}

OGRFeatureDefn *Table::getDefinition() const
{
    if(nullptr != m_layer)
        return m_layer->GetLayerDefn ();
    return nullptr;
}
