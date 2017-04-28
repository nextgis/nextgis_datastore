/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#ifndef NGSSIMPLEDATASET_H
#define NGSSIMPLEDATASET_H

#include "dataset.h"

namespace ngs {

class SimpleDataset : public Dataset
{
public:
    SimpleDataset(enum ngsCatalogObjectType subType,
                  std::vector<CPLString> siblingFiles,
                  ObjectContainer * const parent = nullptr,
                  const CPLString & name = "",
                  const CPLString & path = "");
    ObjectPtr getInternalObject() const;

    // Object interface
public:
    virtual bool destroy() override;

    // ObjectContainer interface
public:
    virtual bool hasChildren() override;
    virtual bool canCreate(const enum ngsCatalogObjectType) const override { return false; }
    virtual bool canPaste(const enum ngsCatalogObjectType) const override { return false; }

    // Dataset interface
    enum ngsCatalogObjectType getSubType() const { return m_subType; }

protected:
    virtual void fillFeatureClasses() override;

private:
    enum ngsCatalogObjectType m_subType;
    std::vector<CPLString> m_siblingFiles;
};

}

#endif // NGSSIMPLEDATASET_H
