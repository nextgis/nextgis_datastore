/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2019 NextGIS, <info@nextgis.com>
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

/**
 * StoreObject
 */
class StoreObject
{
public:
    StoreObject(OGRLayer *layer);
    virtual ~StoreObject() = default;
    virtual FeaturePtr getFeatureByRemoteId(GIntBig rid) const;
    virtual bool setFeatureAttachmentRemoteId(GIntBig aid, GIntBig rid);
    std::vector<ngsEditOperation> fillEditOperations(
            OGRLayer *editHistoryTable) const;

    // static
public:
    static void setRemoteId(FeaturePtr feature, GIntBig rid);
    static GIntBig getRemoteId(FeaturePtr feature);

protected:
    GIntBig getAttachmentRemoteId(GIntBig aid) const;

protected:
    OGRLayer *m_storeIntLayer;
};

/**
 * StoreTable
 */
class StoreTable : public Table, public StoreObject
{
public:
    StoreTable(OGRLayer *layer, ObjectContainer * const parent = nullptr,
               const std::string &name = "");
    virtual ~StoreTable() override = default;

    // Table interface
public:
    virtual std::vector<AttachmentInfo> attachments(GIntBig fid) const override;
    virtual GIntBig addAttachment(GIntBig fid, const std::string &fileName,
                                  const std::string &description,
                                  const std::string &filePath,
                                  const Options &options = Options(),
                                  bool logEdits = true) override;
    virtual bool setProperty(const std::string &key, const std::string &value,
                             const std::string &domain) override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                               const std::string &domain) const override;
    virtual Properties properties(const std::string &domain) const override;
    virtual void deleteProperties(const std::string &domain) override;
    virtual std::vector<ngsEditOperation> editOperations() const override;

    // Table interface
protected:
    virtual FeaturePtr logEditFeature(FeaturePtr feature, FeaturePtr attachFeature,
                                      enum ngsChangeCode code) override;

protected:
    virtual void fillFields() const override;
};

/**
 * StoreFeatureClass
 */
class StoreFeatureClass : public FeatureClass, public StoreObject
{
public:
    StoreFeatureClass(OGRLayer *layer, ObjectContainer * const parent = nullptr,
                      const std::string &name = "");
    virtual ~StoreFeatureClass() override = default;

    // Table interface
public:
    virtual std::vector<AttachmentInfo> attachments(GIntBig fid) const override;
    virtual GIntBig addAttachment(GIntBig fid, const std::string &fileName,
                                  const std::string &description,
                                  const std::string &filePath,
                                  const Options  &options = Options(),
                                  bool logEdits = true) override;
    virtual bool setProperty(const std::string &key, const std::string &value,
                             const std::string &domain) override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual Properties properties(const std::string &domain) const override;
    virtual void deleteProperties(const std::string &domain) override;
    virtual std::vector<ngsEditOperation> editOperations() const override;

    // Table interface
protected:
    virtual FeaturePtr logEditFeature(FeaturePtr feature, FeaturePtr attachFeature,
                                      enum ngsChangeCode code) override;

protected:
    virtual void fillFields() const override;
};

/**
 * TracksTable
 */

typedef struct _TrackInfo {
    std::string name;
    long startTimeStamp;
    long stopTimeStamp;
    long count;
} TrackInfo;

class TrackPointsTable : public FeatureClass
{
public:
    TrackPointsTable(OGRLayer *layer, ObjectContainer * const parent = nullptr);
    virtual ~TrackPointsTable() override;

    // Object interface
public:
    virtual ObjectPtr pointer() const override;
};

class TracksTable : public FeatureClass
{
public:
    TracksTable(OGRLayer *linesLayer, OGRLayer *pointsLayer, ObjectContainer * const parent = nullptr);
    virtual ~TracksTable() override;

    void sync(int maxPointCount = 100);
    std::vector<TrackInfo> getTracks();
    bool addPoint(const std::string &name, double x, double y, double z, float accuracy, float speed, float course,
            long timeStamp, int satCount, bool newTrack, bool newSegment);
    void deletePoints(long start, long end);
    ObjectPtr getPointsLayer() const;

    // Object interface
public:
    virtual ObjectPtr pointer() const override;
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool destroy() override;

private:
    bool flashBuffer();

private:
    int m_lastTrackId;
    int m_lastSegmentId;
    int m_lastSegmentPtId;
    Mutex m_syncMutex, m_bufferMutex;
    std::vector<FeaturePtr> mPointBuffer;
    FeaturePtr m_currentTrack;
    long m_lastGmtTimeStamp;
    bool m_newTrack;
    GIntBig m_pointCount;
    FeatureClassPtr m_pointsLayer;
};

} // namespace ngs

#endif // NGSSTOREFEATURECLASS_H
