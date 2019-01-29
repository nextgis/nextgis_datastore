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

#include "overlay.h"

#include <iterator>

#include "ds/geometry.h"
#include "map/gl/layer.h"
#include "map/mapview.h"
#include "util/error.h"
#include "util/settings.h"

namespace ngs {

constexpr double TOLERANCE_PX = 7.0;
constexpr double GEOMETRY_SIZE_PX = 50.0;

//------------------------------------------------------------------------------
// Overlay
//------------------------------------------------------------------------------

Overlay::Overlay(MapView *map, ngsMapOverlayType type) : m_map(map),
    m_type(type),
    m_visible(false)
{
}


bool Overlay::setOptions(const Options &options)
{
    ngsUnused(options);
    return false;
}

//------------------------------------------------------------------------------
// LocationOverlay
//------------------------------------------------------------------------------
LocationOverlay::LocationOverlay(MapView *map) : Overlay(map, MOT_LOCATION),
    m_location({0.0f, 0.0f}),
    m_direction(0.0f),
    m_accuracy(0.0f)
{
}

void LocationOverlay::setLocation(const ngsCoordinate &location, float direction,
                                  float accuracy)
{
    m_location.x = static_cast<float>(location.X);
    m_location.y = static_cast<float>(location.Y);
    m_direction = direction;
    m_accuracy = accuracy;
    if(!m_visible) {
        m_visible = true;
    }
}

//------------------------------------------------------------------------------
// EditLayerOverlay
//------------------------------------------------------------------------------

EditLayerOverlay::EditLayerOverlay(MapView *map) : Overlay(map, MOT_EDIT),
    m_editFeatureId(NOT_FOUND),
    m_walkingMode(false),
    m_crossVisible(false)
{
    const Settings &settings = Settings::instance();
    m_tolerancePx = settings.getDouble("map/overlay/edit/tolerance", TOLERANCE_PX);
}

bool EditLayerOverlay::undo()
{
    if(m_editGeometry) {
        return m_editGeometry->undo();
    }
    return false;
}

bool EditLayerOverlay::redo()
{
    if(m_editGeometry) {
        return m_editGeometry->undo();
    }
    return false;
}

bool EditLayerOverlay::canUndo() const
{
    if(m_editGeometry) {
        return m_editGeometry->canUndo();
    }
    return false;
}

bool EditLayerOverlay::canRedo() const
{
    if(m_editGeometry) {
        return m_editGeometry->canRedo();
    }
    return false;
}

FeaturePtr EditLayerOverlay::save()
{
    if(!m_featureClass) {
        errorMessage(_("Feature class is null"));
        return FeaturePtr();
    }


    // If geometry is empty delete a feature.
    if(m_editGeometry && !m_editGeometry->isValid()) {
        m_editGeometry.reset();
    }

    bool deleteGeometry = (!m_editGeometry);
    bool featureHasEdits = (m_editFeatureId >= 0);

    FeaturePtr feature;
    OGRGeometry *geom = nullptr;
    if(featureHasEdits && deleteGeometry) { // Delete a feature.
        bool featureDeleted = m_featureClass->deleteFeature(m_editFeatureId);
        if(!featureDeleted) {
            errorMessage(_("Delete feature %ld failed"), m_editFeatureId);
            return FeaturePtr();
        }
    }
    else if(!deleteGeometry) { // Insert or update a feature.
        feature = (featureHasEdits) ?
                    m_featureClass->getFeature(m_editFeatureId) :
                    m_featureClass->createFeature();

        geom = m_editGeometry->toGDALGeometry();
        if(OGRERR_NONE != feature->SetGeometryDirectly(geom)) {
            delete geom;
            errorMessage(_("Set geometry to feature failed"));
            return FeaturePtr();
        }

        bool featureSaved = false;
        if(featureHasEdits) {
            featureSaved = m_featureClass->updateFeature(feature);
        }
        else {
            featureSaved = m_featureClass->insertFeature(feature);
        }
        if(!featureSaved) {
            errorMessage(_("Save feature failed"));
            return FeaturePtr();
        }
    }

    if(m_editLayer) { // Show a hidden ids.
        GlSelectableFeatureLayer *featureLayer =
                ngsDynamicCast(GlSelectableFeatureLayer, m_editLayer);
        if(featureLayer) {
            m_editFeatureId = NOT_FOUND;
            featureLayer->setHideIds(); // Empty hidden ids.
        }
    }

    if(geom) { // Redraw a geometry tile.
        OGREnvelope ogrEnv;
        geom->getEnvelope(&ogrEnv);
        m_map->invalidate(Envelope(ogrEnv));
    }
    else {
        // If geometry deleted invalidate it's original envelope.
        m_map->invalidate(Envelope(-0.5, -0.5, 0.5, 0.5));
    }

    init();
    setVisible(false);
    return feature;
}

void EditLayerOverlay::cancel()
{
    if(m_editLayer) {
        GlSelectableFeatureLayer *featureLayer =
                ngsDynamicCast(GlSelectableFeatureLayer, m_editLayer);
        if(!featureLayer) {
            errorMessage(_("Feature layer is null"));
            return;
        }
        m_editFeatureId = NOT_FOUND;
        featureLayer->setHideIds(); // Empty hidden ids.
        m_map->invalidate(Envelope());
    }

    init();
    setVisible(false);
}

OGRGeometry *EditLayerOverlay::geometry() const
{
    if(m_editGeometry) {
        return m_editGeometry->toGDALGeometry();
    }
    return nullptr;
}

bool EditLayerOverlay::createGeometry(const FeatureClassPtr &datasource, bool empty)
{
    m_featureClass = datasource;
    return createGeometry(m_featureClass->geometryType(), empty);
}

bool EditLayerOverlay::createGeometry(OGRwkbGeometryType type, bool empty)
{
    m_editLayer.reset();
    m_editFeatureId = NOT_FOUND;

    OGRRawPoint geometryCenter = m_map->getCenter();
    OGRRawPoint mapDist = m_map->getMapDistance(GEOMETRY_SIZE_PX,
                                                GEOMETRY_SIZE_PX);

    switch(OGR_GT_Flatten(type)) {
        case wkbPoint:
            m_editGeometry = EditGeometryUPtr(new EditPoint(geometryCenter.x,
                                                            geometryCenter.y));
            break;

        case wkbLineString: {
            EditLine *line = new EditLine;
            if(!empty) {
                line->init(geometryCenter.x - mapDist.x,
                           geometryCenter.y - mapDist.y,
                           geometryCenter.x + mapDist.x,
                           geometryCenter.y + mapDist.y);
            }
            m_editGeometry = EditGeometryUPtr(line);
            break;
        }

        case wkbPolygon: {
            EditPolygon *poly = new EditPolygon;
            if(!empty) {
                double x1 = geometryCenter.x - mapDist.x;
                double y1 = geometryCenter.y - mapDist.y;
                double x2 = geometryCenter.x + mapDist.x;
                double y2 = geometryCenter.y + mapDist.y;

                poly->init(x1, y1, x2, y2);
            }
            m_editGeometry = EditGeometryUPtr(poly);
            break;
        }

        case wkbMultiPoint: {
            EditMultiPoint *mpoint = new EditMultiPoint;
            if(!empty) {
                mpoint->init(geometryCenter.x, geometryCenter.y);
            }
            m_editGeometry = EditGeometryUPtr(mpoint);
            break;
        }

        case wkbMultiLineString: {
            EditMultiLine *mline = new EditMultiLine;
            if(!empty) {
                mline->init(geometryCenter.x - mapDist.x,
                             geometryCenter.y - mapDist.y,
                             geometryCenter.x + mapDist.x,
                             geometryCenter.y + mapDist.y);
            }
            m_editGeometry = EditGeometryUPtr(mline);
            break;
        }

        case wkbMultiPolygon: {
            EditMultiPolygon *mpoly = new EditMultiPolygon;
            if(!empty) {
                double x1 = geometryCenter.x - mapDist.x;
                double y1 = geometryCenter.y - mapDist.y;
                double x2 = geometryCenter.x + mapDist.x;
                double y2 = geometryCenter.y + mapDist.y;

                mpoly->init(x1, y1, x2, y2);
            }
            m_editGeometry = EditGeometryUPtr(mpoly);
            break;
        }

        default: {
            return errorMessage(_("Geometry type is unsupportd"));
        }
    }

    setVisible(true);
    return true;
}

bool EditLayerOverlay::editGeometry(const LayerPtr &layer, GIntBig featureId)
{
    bool useLayerParam = (layer.get());
    m_editLayer = layer;

    FeatureLayer *featureLayer = nullptr;
    if(m_editLayer) {
        featureLayer = ngsDynamicCast(FeatureLayer, m_editLayer);
    }
    else {
        int layerCount = static_cast<int>(m_map->layerCount());
        for(int i = 0; i < layerCount; ++i) {
            m_editLayer = m_map->getLayer(i);
            featureLayer = ngsDynamicCast(FeatureLayer, m_editLayer);
            if(featureLayer && featureLayer->hasSelectedIds()) {
                // Get selection from first layer which has selected ids.
                break;
            }
        }
    }

    if(!featureLayer) {
        return errorMessage(_("Render layer is null"));
    }

    m_featureClass = std::dynamic_pointer_cast<FeatureClass>(
                featureLayer->datasource());
    if(!m_featureClass) {
        return errorMessage(_("Layer datasource is null"));
    }

    if(useLayerParam && featureId > 0) {
        m_editFeatureId = featureId;
    }
    else {
        // Get first selected feature.
        m_editFeatureId = *featureLayer->selectedIds().cbegin();
    }

    FeaturePtr feature = m_featureClass->getFeature(m_editFeatureId);
    if(!feature) {
        return errorMessage(_("Feature is null"));
    }

    OGRGeometry *gdalGeom = feature->GetGeometryRef();
    m_editGeometry = EditGeometryUPtr(EditGeometry::fromGDALGeometry(gdalGeom));
    if(!m_editGeometry) {
        return errorMessage(_("Geometry is null"));
    }

    std::set<GIntBig> hideIds;
    hideIds.insert(m_editFeatureId);
    featureLayer->setHideIds(hideIds);

    OGREnvelope ogrEnv;
    gdalGeom->getEnvelope(&ogrEnv);
    m_map->invalidate(Envelope(ogrEnv));

    setVisible(true);
    return true;
}

bool EditLayerOverlay::deleteGeometry()
{
    if(!m_editGeometry) {
        return false;
    }

    m_editGeometry.reset();
    return save();
}

bool EditLayerOverlay::createPoint()
{
    if(!m_editGeometry) {
        return false;
    }
    OGRRawPoint geometryCenter = m_map->getCenter();
    return m_editGeometry->addPoint(geometryCenter.x, geometryCenter.y);
}

bool EditLayerOverlay::addPoint(double x, double y)
{
    if(!m_editGeometry) {
        return false;
    }

    return m_editGeometry->addPoint(x, y, m_walkingMode ? false : true);
}

enum ngsEditDeleteResult EditLayerOverlay::deletePoint()
{
    if(!m_editGeometry) {
        return EDT_FAILED;
    }

    return m_editGeometry->deletePiece(EditGeometry::PieceType::POINT);
}

bool EditLayerOverlay::addHole()
{
    if(!m_editGeometry) {
        return false;
    }

    OGRRawPoint geometryCenter = m_map->getCenter();
    OGRRawPoint mapDist =
            m_map->getMapDistance(GEOMETRY_SIZE_PX, GEOMETRY_SIZE_PX);
    double x1 = geometryCenter.x - mapDist.x;
    double y1 = geometryCenter.y - mapDist.y;
    double x2 = geometryCenter.x + mapDist.x;
    double y2 = geometryCenter.y + mapDist.y;

    return m_editGeometry->addPiece(EditGeometry::PieceType::HOLE, x1, y1, x2, y2);
}

enum ngsEditDeleteResult EditLayerOverlay::deleteHole()
{
    if(!m_editGeometry) {
        return EDT_FAILED;
    }

    return m_editGeometry->deletePiece(EditGeometry::PieceType::HOLE);
}

bool EditLayerOverlay::addGeometryPart()
{
    if(!m_editGeometry) {
        return false;
    }

    OGRRawPoint geometryCenter = m_map->getCenter();
    OGRRawPoint mapDist =
            m_map->getMapDistance(GEOMETRY_SIZE_PX, GEOMETRY_SIZE_PX);
    double x1 = geometryCenter.x - mapDist.x;
    double y1 = geometryCenter.y - mapDist.y;
    double x2 = geometryCenter.x + mapDist.x;
    double y2 = geometryCenter.y + mapDist.y;

    return m_editGeometry->addPiece(EditGeometry::PieceType::PART, x1, y1, x2, y2);
}

enum ngsEditDeleteResult EditLayerOverlay::deleteGeometryPart()
{
    if(!m_editGeometry) {
        return EDT_FAILED;
    }

    return m_editGeometry->deletePiece(EditGeometry::PieceType::PART);
}

bool EditLayerOverlay::setOptions(const Options &options)
{
    bool cross = options.asBool("CROSS", false);
    setCrossVisible(cross);
    return true;
}

Options EditLayerOverlay::options() const
{
    Options options;
    options.add("CROSS", (m_crossVisible) ? "ON" : "OFF");
    return options;
}

ngsPointId EditLayerOverlay::touch(double x, double y, enum ngsMapTouchType type)
{
    OGRRawPoint mapPt = m_map->displayToWorld(OGRRawPoint(x, y));
    CPLDebug("ngstore", "display x: %f, display y: %f, touch type: %d, map x: %f, map y: %f",
             x, y, type, mapPt.x, mapPt.y);

    // Get tollerance for carrent map scale
    OGRRawPoint mapTolerance = m_map->getMapDistance(m_tolerancePx, m_tolerancePx);
    return m_editGeometry->touch(mapPt, type,
                                 (mapTolerance.x + mapTolerance.y) / 2);
}

void EditLayerOverlay::init()
{
    m_editLayer.reset();
    m_featureClass.reset();
    m_editFeatureId = NOT_FOUND;
    m_editGeometry.reset();
}

} // namespace ngs
