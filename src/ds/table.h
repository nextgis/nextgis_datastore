/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
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
#ifndef NGSTABLE_H
#define NGSTABLE_H

// gdal
#include "ogrsf_frmts.h"

#include "catalog/object.h"
#include "ngstore/codes.h"

#include <util/mutex.h>
#include <util/options.h>
#include <util/progress.h>

namespace ngs {

constexpr const char* LOG_EDIT_HISTORY_KEY = "LOG_EDIT_HISTORY";

class FieldMapPtr : public std::shared_ptr<int>
{
public:
    explicit FieldMapPtr(unsigned long size);
    int &operator[](int key);
    const int &operator[](int key) const;
};

typedef struct _Field {
    std::string m_name;
    std::string m_originalName;
    std::string m_alias;
    OGRFieldType m_type;
} Field;

class Table;

class FeaturePtr : public std::shared_ptr<OGRFeature>
{
public:
    FeaturePtr(OGRFeature *feature, Table *table = nullptr);
    FeaturePtr(OGRFeature *feature, const Table *table);
    FeaturePtr();
    FeaturePtr &operator=(OGRFeature *feature);
    operator OGRFeature*() const { return get(); }
    Table *table() const { return m_table; }
    void setTable(Table *table) { m_table = table; }
private:
    Table *m_table;
};

using TablePtr = std::shared_ptr<Table>;

class Table : public Object
{
    friend class Dataset;
    friend class Folder;
    friend class StoreObject;
public:
    typedef struct _attachmentInfo {
        GIntBig id;
        std::string name;
        std::string description;
        std::string path;
        GIntBig size;
        GIntBig rid;
    } AttachmentInfo;
public:
    explicit Table(OGRLayer *layer,
          ObjectContainer * const parent = nullptr,
          const enum ngsCatalogObjectType type = CAT_TABLE_ANY,
          const std::string &name = "");
    virtual ~Table() override;
    FeaturePtr createFeature() const;
    FeaturePtr getFeature(GIntBig id) const;
    virtual bool insertFeature(const FeaturePtr &feature, bool logEdits = true);
    virtual bool updateFeature(const FeaturePtr &feature, bool logEdits = true);
    virtual bool deleteFeature(GIntBig id, bool logEdits = true);
    virtual bool deleteFeatures(bool logEdits = true);
    GIntBig featureCount(bool force = false) const;
    void reset() const;
    void setAttributeFilter(const std::string &filter = "");
    FeaturePtr nextFeature() const;
    virtual int copyRows(const TablePtr srcTable,
                         const FieldMapPtr fieldMap,
                         const Progress &progress = Progress());
    std::string fidColumn() const;
    const std::vector<Field> &fields() const;
    virtual GIntBig addAttachment(GIntBig fid, const std::string &fileName,
                          const std::string &description, const std::string &filePath,
                          const Options &options = Options(), bool logEdits = true);
    virtual bool deleteAttachment(GIntBig aid, bool logEdits = true);
    virtual bool deleteAttachments(GIntBig fid, bool logEdits = true);
    virtual bool updateAttachment(GIntBig aid, const std::string &fileName,
                                  const std::string &description,
                                  bool logEdits = true);
    virtual std::vector<AttachmentInfo> attachments(GIntBig fid) const;


    // Edit log
    virtual void deleteEditOperation(const ngsEditOperation &op);
    virtual std::vector<ngsEditOperation> editOperations() const;

    // Object interface
public:
    virtual bool canDestroy() const override;
    virtual bool destroy() override;
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool setProperty(const std::string &name, const std::string &value,
                             const std::string &domain) override;
    virtual void deleteProperties(const std::string &domain) override;

protected:
    OGRFeatureDefn *definition() const;
    bool initAttachmentsTable() const;
    bool initEditHistoryTable() const;
    std::string getAttachmentsPath() const;
    virtual void fillFields() const;
    virtual void logEditOperation(const FeaturePtr &opFeature);
    virtual FeaturePtr logEditFeature(FeaturePtr feature, FeaturePtr attachFeature,
                                      enum ngsChangeCode code);
    virtual void checkSetProperty(const std::string &key, const std::string &value,
                                  const std::string &domain);
    virtual bool saveEditHistory();
    virtual std::string fullPropertyDomain(const std::string &domain) const;


protected:
    OGRLayer *m_layer;
    mutable OGRLayer *m_attTable;
    mutable OGRLayer *m_editHistoryTable;
    int m_saveEditHistory;
    mutable std::vector<Field> m_fields;
    Mutex m_featureMutex;
};

}
#endif // NGSTABLE_H
