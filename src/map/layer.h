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

constexpr const char *LAYER_TYPE_KEY = "type";
constexpr const char *DEFAULT_LAYER_NAME = "new layer";

class Map;

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
    explicit Layer(Map *map, const std::string& name = DEFAULT_LAYER_NAME,
                   enum class Type type = Type::Invalid);
    virtual ~Layer() = default;
    virtual bool load(const CPLJSONObject &store,
                      ObjectContainer *objectContainer = nullptr);
    virtual CPLJSONObject save(const ObjectContainer *objectContainer = nullptr) const;
    virtual ObjectPtr datasource() const { return ObjectPtr();}
    virtual std::string name() const { return m_name; }
    virtual void setName(const std::string &name) { m_name = name; }
    virtual bool visible() const { return m_visible; }
    virtual float minZoom() const { return m_minZoom; }
    virtual float maxZoom() const { return m_maxZoom; }
    virtual void setVisible(bool visible) { m_visible = visible; }
    virtual void setMinZoom(float zoom) { m_minZoom = zoom; }
    virtual void setMaxZoom(float zoom) { m_maxZoom = zoom; }
    Map *map() const { return m_map; }
protected:
    std::string m_name;
    enum class Type m_type;
    bool m_visible;
    float m_minZoom, m_maxZoom;
    Map *m_map;
};

using LayerPtr = std::shared_ptr<Layer>;
using FeatureIDs = std::set<GIntBig>;

class ISelectableFeatureLayer {
public:
    virtual ~ISelectableFeatureLayer() = default;
    virtual void setSelectedIds(const FeatureIDs &selectedIds) {
        m_selectedFIDs.clear();
        m_selectedFIDs.insert(selectedIds.begin(), selectedIds.end());
    }
    virtual const FeatureIDs &selectedIds() const { return m_selectedFIDs; }
    virtual bool hasSelectedIds() const { return !m_selectedFIDs.empty(); }
    virtual void setHideIds(const FeatureIDs& hideIds = FeatureIDs()) {
        m_hideFIDs.clear();
        m_hideFIDs.insert(hideIds.begin(), hideIds.end());
    }
protected:
    FeatureIDs m_selectedFIDs;
    FeatureIDs m_hideFIDs;
};

class IRenderLayer {
public:
    virtual bool setStyleName(const std::string &name) = 0;
    virtual bool setStyle(const CPLJSONObject &style) = 0;
    virtual CPLJSONObject style() const = 0;
    virtual std::string styleName() const = 0;
};

/**
 * @brief The FeatureLayer class Layer with vector features
 */
class FeatureLayer : public Layer, public ISelectableFeatureLayer
{
public:
    explicit FeatureLayer(Map *map, const std::string& name = DEFAULT_LAYER_NAME);
    virtual ~FeatureLayer() override = default;
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
    explicit RasterLayer(Map *map, const std::string& name = DEFAULT_LAYER_NAME);
    virtual ~RasterLayer() override = default;
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
