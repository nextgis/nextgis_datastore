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
#include "catalog/objectcontainer.h"
#include "featureclass.h"

namespace ngs {

class NGWFeatureClass : public FeatureClass, public NGWResourceBase
{
public:
    explicit NGWFeatureClass(ObjectContainer * const parent,
                             const enum ngsCatalogObjectType type,
                             const std::string &name,
                             const CPLJSONObject &resource,
                             NGWConnectionBase *connection);
    explicit NGWFeatureClass(ObjectContainer * const parent,
                             const enum ngsCatalogObjectType type,
                             const std::string &name,
                             GDALDataset *DS,
                             OGRLayer *layer,
                             NGWConnectionBase *connection);
    ~NGWFeatureClass() override;

    //static
public:
    static NGWFeatureClass *createFeatureClass(NGWResourceGroup *resourceGroup,
                                               const std::string &name,
                                               const Options &options,
                                               const Progress &progress = Progress());
    static NGWFeatureClass *createFeatureClass(NGWResourceGroup *resourceGroup,
                                               const std::string &name,
                                               OGRFeatureDefn * const definition,
                                               SpatialReferencePtr spatialRef,
                                               OGRwkbGeometryType type,
                                               const Options &options,
                                               const Progress &progress = Progress());

    // FeatureClass interface
public:
    virtual OGRwkbGeometryType geometryType() const override;
    virtual std::vector<OGRwkbGeometryType> geometryTypes() const override;
    virtual Envelope extent() const override;

    // SpatialDataset interface
public:
    virtual SpatialReferencePtr spatialReference() const override;

    // Object interface
public:
    virtual bool destroy() override;
    virtual bool canDestroy() const override;
    virtual bool rename(const std::string &newName) override;
    virtual bool canRename() const override;
    virtual Properties properties(const std::string &domain) const override;
    virtual std::string property(const std::string &key,
                                 const std::string &defaultValue,
                                 const std::string &domain) const override;
    virtual bool setProperty(const std::string &key,
                             const std::string &value,
                             const std::string &domain) override;
    virtual void deleteProperties(const std::string &domain) override;

    // Table interface
public:
    virtual bool insertFeature(const FeaturePtr &feature, bool logEdits) override;
    virtual bool updateFeature(const FeaturePtr &feature, bool logEdits) override;
    virtual bool deleteFeature(GIntBig id, bool logEdits) override;
    virtual bool deleteFeatures(bool logEdits) override;
    virtual bool onRowsCopied(const Progress &progress,
                              const Options &options) override;

private:
    void openDS() const;
    void close();

private:
    OGRwkbGeometryType m_geometryType;
    mutable GDALDataset *m_DS;
};

} // namespace ngs

#endif // NGSNGWDS_H
