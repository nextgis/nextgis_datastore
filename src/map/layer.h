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

#include "util/jsondocument.h"

// stl
#include <memory>
#include <vector>

#include "catalog/objectcontainer.h"

namespace ngs {

constexpr const char* LAYER_TYPE = "type";

/**
 * @brief The Layer class - base class for any map layer.
 */
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
    virtual ~Layer() = default;
    virtual bool load(const JSONObject& store,
                      const ObjectContainer *objectContainer = nullptr);
    virtual JSONObject save(const ObjectContainer * objectContainer = nullptr) const;
    const CPLString &getName() const { return m_name; }
    void setName(const CPLString &name) { m_name = name; }

protected:
    CPLString m_name;
    enum Type m_type;
};

typedef std::shared_ptr<Layer> LayerPtr;

}

#endif // NGSLAYER_H
