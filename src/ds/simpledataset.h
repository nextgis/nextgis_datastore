/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#ifndef NGSSIMPLEDATASET_H
#define NGSSIMPLEDATASET_H

#include "dataset.h"

namespace ngs {

/**
 * @brief The SingleLayerDataset class Dataset with only one layer. ESRI Shapefile, MapInfo tab, etc.
 */
class SingleLayerDataset : public Dataset
{
public:
    explicit SingleLayerDataset(enum ngsCatalogObjectType subType,
                           ObjectContainer * const parent = nullptr,
                           const std::string &name = "",
                           const std::string &path = "");
    virtual ObjectPtr internalObject();
    enum ngsCatalogObjectType subType() const;

private:
    enum ngsCatalogObjectType m_subType;
};

/**
 * @brief The SimpleDataset class Local file dataset with only one layer. ESRI Shapefile, MapInfo tab, etc.
 */
class SimpleDataset : public SingleLayerDataset
{
public:
    explicit SimpleDataset(enum ngsCatalogObjectType subType,
                           std::vector<std::string> siblingFiles,
                           ObjectContainer * const parent = nullptr,
                           const std::string &name = "",
                           const std::string &path = "");
    std::vector<std::string> siblingFiles() const;

    // Object interface
public:
    virtual bool destroy() override;

    // ObjectContainer interface
public:
    virtual bool hasChildren() const override;
    virtual bool canCreate(const enum ngsCatalogObjectType) const override;
    virtual bool canPaste(const enum ngsCatalogObjectType) const override;

    // Dataset interface
protected:
    virtual GDALDatasetPtr createAdditionsDataset() override;

protected:
    virtual void fillFeatureClasses() const override;

private:
    std::vector<std::string> m_siblingFiles;

};

}

#endif // NGSSIMPLEDATASET_H
