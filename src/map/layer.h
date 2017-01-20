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
#ifndef NGSLAYER_H
#define NGSLAYER_H

#include "ds/datastore.h"
#include "util/jsondocument.h"

// stl
#include <memory>
#include <string>
#include <vector>

namespace ngs {


class Layer
{
public:
    enum class Type {
        Invalid = 0,
        Group,
        Vector,
        Raster
    };

public:
    Layer();
    Layer(const CPLString& name, DatasetPtr dataset);
    virtual ~Layer();
    virtual int load(const JSONObject& store,
                     DatasetContainerPtr dataStore = DatasetContainerPtr(),
                     const CPLString& mapPath = "");
    // TODO: virtual int load(const JSONObject& store, const CPLString& mapPath = "");
    JSONObject save(const CPLString& mapPath) const;
protected:
    CPLString m_name;
    enum Type m_type;
    DatasetPtr m_dataset;
};

typedef std::shared_ptr<Layer> LayerPtr;

}

#endif // NGSLAYER_H
