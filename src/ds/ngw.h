/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2020 NextGIS, <info@nextgis.com>
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
#ifndef NGSNGWDS_H
#define NGSNGWDS_H

#include "catalog/ngw.h"
#include "featureclass.h"
#include "simpledataset.h"

namespace ngs {

/**
 * @brief The NGWSinglLayerDataset class
 */
class NGWLayerDataset : public SingleLayerDataset, public NGWResourceBase
{
public:
    explicit NGWLayerDataset(ObjectContainer * const parent,
                             const enum ngsCatalogObjectType type,
                             const std::string &name,
                             const CPLJSONObject &resource,
                             NGWConnectionBase *connection);
    explicit NGWLayerDataset(ObjectContainer * const parent,
                             const enum ngsCatalogObjectType type,
                             const std::string &name,
                             GDALDatasetPtr DS,
                             OGRLayer *layer,
                             NGWConnectionBase *connection);
    virtual void addResource(const CPLJSONObject &resource);
    virtual ObjectPtr getResource(const std::string &resourceId) const;

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
    virtual bool rename(const std::string &newName) override;
    virtual bool canRename() const override;

    // SingleLayerDataset interface
public:
    virtual ObjectPtr internalObject() override;

    // ObjectContainer interface
public:
    virtual bool canCreate(const enum ngsCatalogObjectType type) const override;
    virtual ObjectPtr create(const enum ngsCatalogObjectType type,
                             const std::string &name, const Options &options) override;

    // DatasetBase interface
public:
    virtual bool open(unsigned int openFlags, const Options &options) override;
    virtual void close() override;

    //static
public:
    static NGWLayerDataset *createFeatureClass(NGWResourceGroup *resourceGroup,
                                               const std::string &name,
                                               const Options &options,
                                               const Progress &progress = Progress());
    static NGWLayerDataset *createFeatureClass(NGWResourceGroup *resourceGroup,
                                               const std::string &name,
                                               OGRFeatureDefn * const definition,
                                               SpatialReferencePtr spatialRef,
                                               OGRwkbGeometryType type,
                                               const Options &options,
                                               const Progress &progress = Progress());

private:
    ObjectPtr m_FC;
};

/**
 * @brief The NGWFeatureClass class
 */
class NGWFeatureClass : public FeatureClass
{
    friend class NGWResourceGroup;
public:
    explicit NGWFeatureClass(ObjectContainer * const parent,
                             const enum ngsCatalogObjectType type,
                             const std::string &name,
                             OGRLayer *layer);

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
    virtual bool rename(const std::string &newName) override;
    virtual bool canRename() const override;

    // Table interface
public:
    virtual std::vector<FeaturePtr::AttachmentInfo> attachments(GIntBig fid) const override;
    virtual bool onRowsCopied(const TablePtr srcTable, const Progress &progress,
                              const Options &options) override;
    virtual GIntBig addAttachment(GIntBig fid, const std::string &fileName,
                                  const std::string &description,
                                  const std::string &filePath,
                                  const Options &options, bool logEdits) override;
    virtual bool deleteAttachment(GIntBig fid, GIntBig aid, bool logEdits) override;
    virtual bool deleteAttachments(GIntBig fid, bool logEdits) override;
    virtual bool updateAttachment(GIntBig fid, GIntBig aid,
                                  const std::string &fileName,
                                  const std::string &description,
                                  bool logEdits) override;
};

} // namespace ngs

#endif // NGSNGWDS_H
