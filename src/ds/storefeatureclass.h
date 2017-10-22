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

constexpr GIntBig INIT_RID_COUNTER = -1000000;

class StoreObject
{
public:
    StoreObject(OGRLayer* layer);
    virtual ~StoreObject() = default;
    virtual FeaturePtr getFeatureByRemoteId(GIntBig rid) const;
    virtual bool setFeatureAttachmentRemoteId(GIntBig aid, GIntBig rid);
    std::vector<ngsEditOperation> fillEditOperations(
            OGRLayer* editHistoryTable) const;

    // static
public:
    static void setRemoteId(FeaturePtr feature, GIntBig rid);
    static GIntBig getRemoteId(FeaturePtr feature);

protected:
    GIntBig getAttachmentRemoteId(GIntBig aid) const;

protected:
    OGRLayer* m_storeIntLayer;
};

class StoreTable : public Table, public StoreObject
{
public:
    StoreTable(OGRLayer* layer, ObjectContainer* const parent = nullptr,
               const CPLString & name = "");
    virtual ~StoreTable() = default;

    // Table interface
public:
    virtual std::vector<AttachmentInfo> attachments(GIntBig fid) override;
    virtual GIntBig addAttachment(GIntBig fid, const char* fileName,
                                  const char* description, const char* filePath,
                                  char** options, bool logEdits = true) override;
    virtual bool setProperty(const char* key, const char* value,
                             const char* domain) override;
    virtual CPLString property(const char* key, const char* defaultValue,
                                  const char* domain) override;
    virtual std::map<CPLString, CPLString> properties(const char* domain) override;
    virtual void deleteProperties() override;
    virtual std::vector<ngsEditOperation> editOperations() override;

    // Table interface
protected:
    virtual FeaturePtr logEditFeature(FeaturePtr feature, FeaturePtr attachFeature,
                                      enum ngsChangeCode code) override;

protected:
    virtual void fillFields() override;
};

class StoreFeatureClass : public FeatureClass, public StoreObject
{
public:
    StoreFeatureClass(OGRLayer* layer, ObjectContainer* const parent = nullptr,
                      const CPLString & name = "");
    virtual ~StoreFeatureClass() = default;

    // Table interface
public:
    virtual std::vector<AttachmentInfo> attachments(GIntBig fid) override;
    virtual GIntBig addAttachment(GIntBig fid, const char* fileName,
                                  const char* description, const char* filePath,
                                  char** options, bool logEdits = true) override;
    virtual bool setProperty(const char* key, const char* value,
                             const char* domain) override;
    virtual CPLString property(const char* key, const char* defaultValue,
                                  const char* domain) override;
    virtual std::map<CPLString, CPLString> properties(const char* domain) override;
    virtual void deleteProperties() override;
    virtual std::vector<ngsEditOperation> editOperations() override;

    // Table interface
protected:
    virtual FeaturePtr logEditFeature(FeaturePtr feature, FeaturePtr attachFeature,
                                      enum ngsChangeCode code) override;

protected:
    virtual void fillFields() override;
};

} // namespace ngs

#endif // NGSSTOREFEATURECLASS_H
