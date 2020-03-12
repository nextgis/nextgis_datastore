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
#ifndef NGSSTORE_H
#define NGSSTORE_H

#include "dataset.h"

namespace ngs {

/**
  * StoreObjectContainer
  */
class StoreObjectContainer
{
public:
    virtual ~StoreObjectContainer() = default;
};

/**
 * StoreObject
 */
class StoreObject
{
public:
    StoreObject(OGRLayer *layer);
    virtual ~StoreObject() = default;
    virtual FeaturePtr getFeatureByRemoteId(GIntBig rid) const;
    std::vector<ngsEditOperation> fillEditOperations(OGRLayer *editHistoryTable,
                                                     Dataset *dataset) const;
    virtual std::string downloadAttachment(GIntBig fid, GIntBig aid,
                                           const Progress &progress = Progress());
    virtual bool setAttachmentRemoteId(GIntBig aid, GIntBig rid);
    GIntBig getAttachmentRemoteId(GIntBig aid) const;
    GIntBig getRemoteId(GIntBig fid) const;

    // static
public:
    static void setRemoteId(FeaturePtr feature, GIntBig rid);
    static GIntBig getRemoteId(FeaturePtr feature);

protected:
    OGRLayer *m_storeIntLayer;
};


} // namespace ngs

#endif // NGSSTORE_H
