/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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

#ifndef NGSOVERLAY_H
#define NGSOVERLAY_H

#include "ds/featureclass.h"
#include "ds/geometry.h"
#include "map/layer.h"
#include "ngstore/codes.h"
#include "ngstore/util/constants.h"

namespace ngs {

class MapView;

/**
 * @brief The Overlay class
 */
class Overlay
{
public:
    explicit Overlay(MapView *map, enum ngsMapOverlayType type = MOT_UNKNOWN);
    virtual ~Overlay() = default;

    enum ngsMapOverlayType type() const { return m_type; }
    virtual void setVisible(bool visible) { m_visible = visible; }
    virtual bool visible() const { return m_visible; }
    virtual bool setOptions(const Options &options);
    virtual Options options() const { return Options(); }

protected:
    MapView *m_map;
    enum ngsMapOverlayType m_type;
    bool m_visible;
};

using OverlayPtr = std::shared_ptr<Overlay>;

/**
 * @brief The EditLayerOverlay class
 */
class EditLayerOverlay : public Overlay
{
public:
    explicit EditLayerOverlay(MapView *map);
    virtual ~EditLayerOverlay() override = default;

    virtual bool undo();
    virtual bool redo();
    bool canUndo() const;
    bool canRedo() const;
    FeaturePtr save();
    void cancel();

    bool isGeometryValid() const { return m_editGeometry != nullptr; }
    OGRGeometry *geometry() const;

    bool createGeometry(const FeatureClassPtr &datasource, bool empty = false);
    virtual bool createGeometry(OGRwkbGeometryType type, bool empty = false);
    virtual bool editGeometry(const LayerPtr &layer, GIntBig featureId);
    virtual bool deleteGeometry();
    virtual bool createPoint();
    virtual bool addPoint(double x, double y);
    virtual ngsEditDeleteResult deletePoint();
    virtual bool addHole();
    virtual enum ngsEditDeleteResult deleteHole();
    virtual bool addGeometryPart();
    virtual enum ngsEditDeleteResult deleteGeometryPart();

    virtual ngsPointId touch(double x, double y, enum ngsMapTouchType type);

    virtual void setCrossVisible(bool visible) { m_crossVisible = visible; }
    virtual bool isCrossVisible() const { return m_crossVisible; }
    virtual void setWalkingMode(bool walkingMode) { m_walkingMode = walkingMode; }
    virtual bool isWalkingMode() const { return m_walkingMode; }
	
	virtual bool setStyleName(enum ngsEditStyleType type, const std::string &name) = 0;
    virtual bool setStyle(enum ngsEditStyleType type, const CPLJSONObject &jsonStyle) = 0;
    virtual CPLJSONObject style(enum ngsEditStyleType type) const = 0;

    // Overlay interface
public:
    virtual bool setOptions(const Options &options) override;
    virtual Options options() const override;

protected:
    virtual void init();

protected:
    LayerPtr m_editLayer;
    FeatureClassPtr m_featureClass;
    GIntBig m_editFeatureId;
    EditGeometryUPtr m_editGeometry;
    double m_tolerancePx;
    bool m_walkingMode;
    bool m_crossVisible;
};

/**
 * @brief The LocationOverlay class
 */
class LocationOverlay : public Overlay
{
public:
    explicit LocationOverlay(MapView *map);
    virtual ~LocationOverlay() = default;
    virtual void setLocation(const ngsCoordinate &location, float direction,
                             float accuracy);
    virtual bool setStyleName(const std::string &name) = 0;
    virtual bool setStyle(const CPLJSONObject &style) = 0;
    virtual CPLJSONObject style() const = 0;
protected:
    SimplePoint m_location;
    float m_direction, m_accuracy;
};

} // namespace ngs

#endif // NGSOVERLAY_H
