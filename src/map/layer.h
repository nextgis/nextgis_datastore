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
#ifndef NGSLAYER_H
#define NGSLAYER_H

// stl
#include <memory>
#include <vector>

// gdal
#include "cpl_json.h"

#include "catalog/objectcontainer.h"
#include "ds/featureclass.h"
#include "ds/raster.h"

namespace ngs {

constexpr const char* LAYER_TYPE_KEY = "type";
constexpr const char* DEFAULT_LAYER_NAME = "new layer";

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
    explicit Layer(const CPLString& name = DEFAULT_LAYER_NAME, enum Type type = Type::Invalid);
    virtual ~Layer() = default;
    virtual bool load(const CPLJSONObject& store,
                      ObjectContainer *objectContainer = nullptr);
    virtual CPLJSONObject save(const ObjectContainer * objectContainer = nullptr) const;
    virtual ObjectPtr datasource() const { return ObjectPtr();}
    virtual const CPLString &getName() const { return m_name; }
    virtual void setName(const CPLString &name) { m_name = name; }
    virtual bool visible() const { return m_visible; }
    virtual void setVisible(bool visible) { m_visible = visible; }
protected:
    CPLString m_name;
    enum Type m_type;
    bool m_visible;
};

typedef std::shared_ptr<Layer> LayerPtr;

/**
 * @brief The FeatureLayer class Layer with vector features
 */
class FeatureLayer : public Layer
{
public:
    explicit FeatureLayer(const CPLString& name = DEFAULT_LAYER_NAME);
    virtual ~FeatureLayer() = default;
    virtual void setFeatureClass(const FeatureClassPtr &featureClass) {
        m_featureClass = featureClass;
    }

    // Layer interface
public:
    virtual bool load(const CPLJSONObject &store, ObjectContainer *objectContainer) override;
    virtual CPLJSONObject save(const ObjectContainer *objectContainer) const override;
    virtual ObjectPtr datasource() const override {
        return std::dynamic_pointer_cast<Object>(m_featureClass);
    }

protected:
    FeatureClassPtr m_featureClass;
};

/**
 * @brief The RasterLayer class Layer with raster
 */
class RasterLayer : public Layer
{
public:
    explicit RasterLayer(const CPLString& name = DEFAULT_LAYER_NAME);
    virtual ~RasterLayer() = default;
    virtual void setRaster(const RasterPtr &raster) {
        m_raster = raster;
    }

    // Layer interface
public:
    virtual bool load(const CPLJSONObject &store, ObjectContainer *objectContainer) override;
    virtual CPLJSONObject save(const ObjectContainer *objectContainer) const override;
    virtual ObjectPtr datasource() const override {
        return std::dynamic_pointer_cast<Object>(m_raster);
    }

protected:
    RasterPtr m_raster;
};

}

#endif // NGSLAYER_H
