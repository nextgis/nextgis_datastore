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
#ifndef NGSSTOREFEATURECLASS_H
#define NGSSTOREFEATURECLASS_H

#include "featureclass.h"

namespace ngs {

class StoreFeatureClass : public FeatureClass
{
public:
    StoreFeatureClass(OGRLayer* layer, ObjectContainer* const parent = nullptr,
                      const CPLString & name = "");
    virtual ~StoreFeatureClass() = default;
    FeaturePtr getFeatureByRemoteId(GIntBig rid) const;
    bool setFeatureAttachmentRemoteId(GIntBig aid, GIntBig rid);

    // static
    static void setRemoteId(FeaturePtr feature, GIntBig rid);
    static GIntBig getRemoteId(FeaturePtr feature);

    // Table interface
public:
    virtual void fillFields() override;
    virtual std::vector<AttachmentInfo> getAttachments(GIntBig fid) override;
    virtual GIntBig addAttachment(GIntBig fid, const char* fileName,
                                  const char* description, const char* filePath,
                                  char** options) override;
};

} // namespace ngs

#endif // NGSSTOREFEATURECLASS_H
