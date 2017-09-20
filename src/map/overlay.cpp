/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
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
constexpr int MAX_UNDO = 10;

//------------------------------------------------------------------------------
// Overlay
//------------------------------------------------------------------------------

Overlay::Overlay(MapView* map, ngsMapOverlayType type) : m_map(map),
    m_type(type),
    m_visible(false)
{
}

//------------------------------------------------------------------------------
// LocationOverlay
//------------------------------------------------------------------------------
LocationOverlay::LocationOverlay(MapView* map) :
    Overlay(map, MOT_LOCATION),
    m_location({0.0f, 0.0f}),
    m_direction(0.0f),
    m_accuracy(0.0f)
{
}

void LocationOverlay::setLocation(const ngsCoordinate& location, float direction,
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

EditLayerOverlay::EditLayerOverlay(MapView* map) : Overlay(map, MOT_EDIT),
    m_editedFeatureId(NOT_FOUND),
    m_selectedPointId(),
    m_selectedPointCoordinates(),
    m_historyState(-1),
    m_isTouchMoved(false),
    m_wasTouchingSelectedPoint(false)
{
    Settings& settings = Settings::instance();
    m_tolerancePx =
            settings.getDouble("map/overlay/edit/tolerance", TOLERANCE_PX);
}

bool EditLayerOverlay::undo()
{
    if(!canUndo())
        return false;
    return restoreFromHistory(--m_historyState);
}

bool EditLayerOverlay::redo()
{
    if(!canRedo())
        return false;
    return restoreFromHistory(++m_historyState);
}

bool EditLayerOverlay::canUndo()
{
    return m_historyState > 0 && m_historyState < static_cast<int>(m_history.size());
}

bool EditLayerOverlay::canRedo()
{
    return m_historyState >= 0 && m_historyState < static_cast<int>(m_history.size()) - 1;
}

void EditLayerOverlay::saveToHistory()
{
    if(!m_geometry) {
        return;
    }

    int stateToDelete = m_historyState + 1;
    if(stateToDelete >= 1 && stateToDelete < static_cast<int>(m_history.size())) {
        auto it = std::next(m_history.begin(), stateToDelete);
        m_history.erase(it, m_history.end());
    }

    if(m_history.size() >= MAX_UNDO + 1)
        m_history.pop_front();

    m_history.push_back(GeometryUPtr(m_geometry->clone()));
    m_historyState = static_cast<int>(m_history.size()) - 1;
}

bool EditLayerOverlay::restoreFromHistory(int historyState)
{
    if(historyState < 0 || historyState >= static_cast<int>(m_history.size()))
        return false;

    auto it = std::next(m_history.begin(), historyState);
    m_geometry = GeometryUPtr((*it)->clone());
    selectFirstPoint();
    return true;
}

void EditLayerOverlay::clearHistory()
{
    m_history.clear();
    m_historyState = -1;
}

bool EditLayerOverlay::save()
{
    if(!m_datasource)
        return errorMessage(_("Datasource is null"));

    if(m_geometry) { // If multi geometry is empty then delete a feature.
        OGRGeometryCollection* multyGeom =
                ngsDynamicCast(OGRGeometryCollection, m_geometry);
        if(multyGeom && multyGeom->getNumGeometries() == 0)
            m_geometry.reset();
    }
    bool hasDeletedGeometry = (!m_geometry);
    bool hasEditedFeature = (m_editedFeatureId >= 0);

    FeaturePtr feature;
    OGRGeometry* geom = nullptr;
    if(hasEditedFeature && hasDeletedGeometry) { // Delete a feature.
        bool featureDeleted = m_datasource->deleteFeature(m_editedFeatureId);
        if(!featureDeleted)
            return errorMessage(_("Feature deleting is failed"));

    } else if(!hasDeletedGeometry) { // Insert or update a feature.
        feature = (hasEditedFeature)
                ? m_datasource->getFeature(m_editedFeatureId)
                : m_datasource->createFeature();

        geom = m_geometry.release();
        if(!geom)
            return errorMessage(_("Geometry is null"));

        if(OGRERR_NONE != feature->SetGeometryDirectly(geom)) {
            delete geom;
            return errorMessage(_("Set geometry to feature is failed"));
        }

        bool featureSaved = false;
        if(hasEditedFeature)
            featureSaved = m_datasource->updateFeature(feature);
        else
            featureSaved = m_datasource->insertFeature(feature);
        if(!featureSaved)
            return errorMessage(_("Feature saving is failed"));
    }

    if(m_editedLayer) { // Unhide a hidden ids.
        GlSelectableFeatureLayer* featureLayer =
                ngsDynamicCast(GlSelectableFeatureLayer, m_editedLayer);
        if(!featureLayer)
            return errorMessage(_("Feature layer is null"));
        m_editedFeatureId = NOT_FOUND;
        featureLayer->setHideIds(std::set<GIntBig>()); // Empty hidden ids.
    }

    if(geom) { // Redraw a geometry tile.
        OGREnvelope ogrEnv;
        geom->getEnvelope(&ogrEnv);
        m_map->invalidate(Envelope(ogrEnv));
    }

    freeResources();
    setVisible(false);
    return true;
}

void EditLayerOverlay::cancel()
{
    if(m_editedLayer) {
        GlSelectableFeatureLayer* featureLayer =
                ngsDynamicCast(GlSelectableFeatureLayer, m_editedLayer);
        if(!featureLayer) {
            errorMessage(_("Feature layer is null"));
            return;
        }
        m_editedFeatureId = NOT_FOUND;
        featureLayer->setHideIds(std::set<GIntBig>()); // Empty hidden ids.
        m_map->invalidate(Envelope());
    }

    freeResources();
    setVisible(false);
}

bool EditLayerOverlay::createGeometry(FeatureClassPtr datasource)
{
    m_datasource = datasource;
    m_editedLayer.reset();
    m_editedFeatureId = NOT_FOUND;

    OGRwkbGeometryType geometryType = m_datasource->geometryType();
    OGRRawPoint geometryCenter = m_map->getCenter();
    OGRRawPoint mapDist =
            m_map->getMapDistance(GEOMETRY_SIZE_PX, GEOMETRY_SIZE_PX);

    GeometryUPtr geometry = GeometryUPtr();
    switch(OGR_GT_Flatten(geometryType)) {
        case wkbPoint: {
            geometry = GeometryUPtr(
                    new OGRPoint(geometryCenter.x, geometryCenter.y));
            break;
        }
        case wkbLineString: {
            OGRLineString* line = new OGRLineString();
            line->addPoint(
                    geometryCenter.x - mapDist.x, geometryCenter.y - mapDist.y);
            line->addPoint(
                    geometryCenter.x + mapDist.x, geometryCenter.y + mapDist.y);
            geometry = GeometryUPtr(line);

            // FIXME: for test, remove it.
            line->addPoint(geometryCenter.x + 2 * mapDist.x,
                    geometryCenter.y - 2 * mapDist.y);
            break;
        }
        case wkbMultiPoint: {
            OGRMultiPoint* mpt = new OGRMultiPoint();
            mpt->addGeometryDirectly(
                    new OGRPoint(geometryCenter.x, geometryCenter.y));
            geometry = GeometryUPtr(mpt);
            break;
        }
        case wkbMultiLineString: {
            OGRMultiLineString* mline = new OGRMultiLineString();

            OGRLineString* line = new OGRLineString();
            line->addPoint(
                    geometryCenter.x - mapDist.x, geometryCenter.y - mapDist.y);
            line->addPoint(
                    geometryCenter.x + mapDist.x, geometryCenter.y + mapDist.y);

            // FIXME: for test, remove it.
            line->addPoint(geometryCenter.x + 2 * mapDist.x,
                    geometryCenter.y - 2 * mapDist.y);

            mline->addGeometryDirectly(line);
            geometry = GeometryUPtr(mline);

            break;
        }
        default: {
            break;
        }
    }

    setGeometry(std::move(geometry));
    if(!m_geometry) {
        return errorMessage(_("Geometry is null"));
    }
    setVisible(true);
    return true;
}

bool EditLayerOverlay::editGeometry(LayerPtr layer, GIntBig featureId)
{
    bool useLayerParam = (layer.get());
    m_editedLayer = layer;

    GlSelectableFeatureLayer* featureLayer = nullptr;
    if(m_editedLayer) {
        featureLayer = ngsDynamicCast(GlSelectableFeatureLayer, m_editedLayer);
    } else {
        int layerCount = static_cast<int>(m_map->layerCount());
        for(int i = 0; i < layerCount; ++i) {
            layer = m_map->getLayer(i);
            featureLayer = ngsDynamicCast(GlSelectableFeatureLayer, layer);
            if(featureLayer && featureLayer->hasSelectedIds()) {
                // Get selection from first layer which has selected ids.
                m_editedLayer = layer;
                break;
            }
        }
    }

    if(!featureLayer)
        return errorMessage(_("Render layer is null"));

    m_datasource =
            std::dynamic_pointer_cast<FeatureClass>(featureLayer->datasource());
    if(!m_datasource)
        return errorMessage(_("Layer datasource is null"));

    if(useLayerParam && featureId > 0) {
        m_editedFeatureId = featureId;
    } else {
        // Get first selected feature.
        m_editedFeatureId = *featureLayer->selectedIds().cbegin();
    }

    FeaturePtr feature = m_datasource->getFeature(m_editedFeatureId);
    if(!feature)
        return errorMessage(_("Feature is null"));

    GeometryUPtr geometry = GeometryUPtr(feature->GetGeometryRef()->clone());
    setGeometry(std::move(geometry));
    if(!m_geometry) {
        return errorMessage(_("Geometry is null"));
    }

    std::set<GIntBig> hideIds;
    hideIds.insert(m_editedFeatureId);
    featureLayer->setHideIds(hideIds);

    OGREnvelope ogrEnv;
    m_geometry->getEnvelope(&ogrEnv);
    m_map->invalidate(Envelope(ogrEnv));

    setVisible(true);
    return true;
}

bool EditLayerOverlay::deleteGeometry()
{
    if(!m_geometry)
        return false;

    m_geometry.reset();
    m_selectedPointId = PointId();
    m_selectedPointCoordinates = OGRPoint();
    return save();
}

bool EditLayerOverlay::addPoint()
{
    if(!m_geometry)
        return false;

    OGRLineString* line; // only to line
    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbLineString: {
            line = ngsDynamicCast(OGRLineString, m_geometry);
            break;
        }
        case wkbMultiLineString: {
            OGRMultiLineString* ml =
                    ngsDynamicCast(OGRMultiLineString, m_geometry);
            if(!ml)
                break;
            line = dynamic_cast<OGRLineString*>(
                    ml->getGeometryRef(m_selectedPointId.geometryId()));
            break;
        }
        default:
            break; // Not supported yet
    }
    if(!line)
        return false;

    int id = line->getNumPoints() - 1; // Add after the last point.
    OGRRawPoint geometryCenter = m_map->getCenter();
    OGRPoint pt(geometryCenter.x, geometryCenter.y);
    int addedPtId = addPoint(line, id, pt);
    saveToHistory();

    m_selectedPointId.setPointId(addedPtId); // Update only pointId.
    m_selectedPointCoordinates = pt;
    return true;
}

int EditLayerOverlay::addPoint(OGRLineString* line, int id, const OGRPoint& pt)
{
    bool toLineEnd = (line->getNumPoints() - 1 == id);
    if(toLineEnd) {
        line->addPoint(&pt);
        return line->getNumPoints() - 1;
    }

    int addedPtId = id + 1;
    OGRLineString* newLine = new OGRLineString();
    newLine->addSubLineString(line, 0, id);
    newLine->addPoint(&pt);
    newLine->addSubLineString(line, addedPtId);

    line->empty();
    line->addSubLineString(newLine);
    delete newLine;
    return addedPtId;
}

bool EditLayerOverlay::deletePoint()
{
    if(!m_geometry)
        return false;

    bool ret = false;
    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) { // only lines
        case wkbLineString: {
            OGRLineString* line = ngsDynamicCast(OGRLineString, m_geometry);
            if(!line)
                break;

            int minNumPoint = line->get_IsClosed() ? 3 : 2;
            if(line->getNumPoints() <= minNumPoint)
                break;

            OGRLineString* newLine = new OGRLineString();

            bool isStartPoint = (0 == m_selectedPointId.pointId());
            if(!isStartPoint)
                newLine->addSubLineString(
                        line, 0, m_selectedPointId.pointId() - 1);
            newLine->addSubLineString(line, m_selectedPointId.pointId() + 1);

            m_geometry = GeometryUPtr(newLine);
            saveToHistory();

            if(!isStartPoint)
                m_selectedPointId.setPointId(m_selectedPointId.pointId() - 1);

            if(m_selectedPointId) {
                OGRPoint pt;
                line->getPoint(m_selectedPointId.pointId(), &pt);
                m_selectedPointCoordinates = pt;
            } else {
                m_selectedPointId = PointId();
                m_selectedPointCoordinates = OGRPoint();
            }
            ret = true;
            break;
        }
        default: {
            break;
        }
    }
    return ret;
}

bool EditLayerOverlay::addGeometryPart()
{
    if(!m_geometry)
        return false;

    OGRRawPoint geometryCenter = m_map->getCenter();
    OGRRawPoint mapDist =
            m_map->getMapDistance(GEOMETRY_SIZE_PX, GEOMETRY_SIZE_PX);
    bool ret = false;

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) { // only multi
        case wkbMultiPoint: {
            OGRMultiPoint* mpt = ngsDynamicCast(OGRMultiPoint, m_geometry);
            if(!mpt)
                break;

            OGRPoint* pt = new OGRPoint(geometryCenter.x, geometryCenter.y);

            if(OGRERR_NONE != mpt->addGeometryDirectly(pt)) {
                delete pt;
                break;
            }

            int num = mpt->getNumGeometries();
            m_selectedPointId = PointId(0, NOT_FOUND, num - 1);
            m_selectedPointCoordinates = *pt;
            ret = true;
            break;
        }
        case wkbMultiLineString: {
            OGRMultiLineString* mline = ngsDynamicCast(OGRMultiLineString, m_geometry);
            if(!mline)
                break;

            OGRPoint startPt(
                    geometryCenter.x - mapDist.x, geometryCenter.y - mapDist.y);
            OGRPoint endPt(
                    geometryCenter.x + mapDist.x, geometryCenter.y + mapDist.y);
            OGRLineString* line = new OGRLineString();
            line->addPoint(&startPt);
            line->addPoint(&endPt);

            if(OGRERR_NONE != mline->addGeometryDirectly(line)) {
                delete line;
                break;
            }

            int num = mline->getNumGeometries();
            m_selectedPointId = PointId(0, NOT_FOUND, num - 1);
            m_selectedPointCoordinates = startPt;
            ret = true;
            break;
        }
        default: {
            break;
        }
    }

    if(ret) {
        saveToHistory();
    }
    return ret;
}

bool EditLayerOverlay::deleteGeometryPart()
{
    bool deletedLastPart = false;
    if(!m_geometry)
        return deletedLastPart;

    OGRGeometryCollection* collect =
            ngsDynamicCast(OGRGeometryCollection, m_geometry);
    if(!collect || collect->getNumGeometries() == 0) // only multi
        return deletedLastPart;

    if(OGRERR_NONE
            != collect->removeGeometry(m_selectedPointId.geometryId())) {
        return deletedLastPart;
    }
    saveToHistory();

    int lastGeomId = collect->getNumGeometries() - 1;
    if(lastGeomId < 0) {
        deletedLastPart = true;
        m_selectedPointId = PointId();
        m_selectedPointCoordinates = OGRPoint();
    }

    if(!deletedLastPart) {
        const OGRGeometry* lastGeom = collect->getGeometryRef(lastGeomId);
        switch(OGR_GT_Flatten(m_geometry->getGeometryType())) { // only multi
            case wkbMultiPoint: {
                OGRMultiPoint* mpt = ngsDynamicCast(OGRMultiPoint, m_geometry);
                if(!mpt)
                    break;

                const OGRPoint* lastPt =
                        dynamic_cast<const OGRPoint*>(lastGeom);
                m_selectedPointId = PointId(0, NOT_FOUND, lastGeomId);
                m_selectedPointCoordinates = *lastPt;
                break;
            }
            case wkbMultiLineString: {
                OGRMultiLineString* mline =
                        ngsDynamicCast(OGRMultiLineString, m_geometry);
                if(!mline)
                    break;

                const OGRLineString* lastLine =
                        dynamic_cast<const OGRLineString*>(lastGeom);
                int lastPointId = lastLine->getNumPoints() - 1;
                OGRPoint lastPt;
                lastLine->getPoint(lastPointId, &lastPt);
                m_selectedPointId = PointId(lastPointId, NOT_FOUND, lastGeomId);
                m_selectedPointCoordinates = lastPt;
                break;
            }
            default: {
                break;
            }
        }
    }

    return deletedLastPart;
}

void EditLayerOverlay::setGeometry(GeometryUPtr geometry)
{
    m_geometry = std::move(geometry);
    clearHistory();
    saveToHistory();
    selectFirstPoint();
}

ngsPointId EditLayerOverlay::touch(
        double x, double y, enum ngsMapTouchType type)
{
    bool returnSelectedPoint = false;
    switch(type) {
        case MTT_ON_DOWN: {
            m_touchStartPoint = OGRRawPoint(x, y);
            OGRRawPoint mapPt = m_map->displayToWorld(
                    OGRRawPoint(m_touchStartPoint.x, m_touchStartPoint.y));
            if(!m_map->getYAxisInverted()) {
                mapPt.y = -mapPt.y;
            }
            m_wasTouchingSelectedPoint = hasSelectedPoint(&mapPt);
            if(m_wasTouchingSelectedPoint) {
                returnSelectedPoint = true;
            }
            break;
        }
        case MTT_ON_MOVE: {
            if(!m_isTouchMoved) {
                m_isTouchMoved = true;
            }

            OGRRawPoint pt = OGRRawPoint(x, y);
            OGRRawPoint offset(
                    pt.x - m_touchStartPoint.x, pt.y - m_touchStartPoint.y);
            OGRRawPoint mapOffset = m_map->getMapDistance(offset.x, offset.y);
            if(!m_map->getYAxisInverted()) {
                mapOffset.y = -mapOffset.y;
            }

            if(m_wasTouchingSelectedPoint) {
                shiftPoint(mapOffset);
                returnSelectedPoint = true;
            }

            m_touchStartPoint = pt;
            break;
        }
        case MTT_ON_UP: {
            if(m_isTouchMoved) {
                m_isTouchMoved = false;
                if(m_wasTouchingSelectedPoint) {
                    saveToHistory();
                    m_wasTouchingSelectedPoint = false;
                }
            } else{
                OGRRawPoint mapPt = m_map->displayToWorld(
                        OGRRawPoint(m_touchStartPoint.x, m_touchStartPoint.y));
                if(!m_map->getYAxisInverted()) {
                    mapPt.y = -mapPt.y;
                }
                if(clickPoint(mapPt)) {
                    returnSelectedPoint = true;
                }
            }
            break;
        }
        default:
            break;
    }

    ngsPointId ptId;
    if(returnSelectedPoint) {
        ptId.pointId = m_selectedPointId.pointId();
        ptId.isHole = m_selectedPointId.ringId() >= 1;
    } else {
        ptId = {NOT_FOUND, 0};
    }
    return ptId;
}

bool EditLayerOverlay::clickPoint(const OGRRawPoint& mapCoordinates)
{
    return selectPoint(false, mapCoordinates)
            || clickMedianPoint(mapCoordinates);
}

bool EditLayerOverlay::selectFirstPoint()
{
    return selectPoint(true, OGRRawPoint());
}

bool EditLayerOverlay::selectPoint(
        bool selectFirstPoint, const OGRRawPoint& mapCoordinates)
{
    if(m_geometry) {
        OGRPoint coordinates;
        PointId id;

        if(selectFirstPoint) {
            id = getGeometryPointId(
                    *m_geometry, m_map->getExtentLimit(), &coordinates);
        } else {
            OGRRawPoint mapTolerance =
                    m_map->getMapDistance(m_tolerancePx, m_tolerancePx);

            double minX = mapCoordinates.x - mapTolerance.x;
            double maxX = mapCoordinates.x + mapTolerance.x;
            double minY = mapCoordinates.y - mapTolerance.y;
            double maxY = mapCoordinates.y + mapTolerance.y;
            Envelope mapEnv(minX, minY, maxX, maxY);

            id = getGeometryPointId(*m_geometry, mapEnv, &coordinates);
        }

        if(id) {
            m_selectedPointId = id;
            m_selectedPointCoordinates = coordinates;
            return true;
        }
    }
    return false;
}

bool EditLayerOverlay::clickMedianPoint(const OGRRawPoint& mapCoordinates)
{
    if(!m_geometry)
        return false;

    OGRLineString* line;
    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbLineString: {
            line = ngsDynamicCast(OGRLineString, m_geometry);
            break;
        }
        case wkbMultiLineString: {
            OGRMultiLineString* ml =
                    ngsDynamicCast(OGRMultiLineString, m_geometry);
            if(!ml)
                break;
            line = dynamic_cast<OGRLineString*>(
                    ml->getGeometryRef(m_selectedPointId.geometryId()));
            break;
        }
        default:
            break; // Not supported yet
    }
    if(!line)
        return false;

    OGRRawPoint mapTolerance =
            m_map->getMapDistance(m_tolerancePx, m_tolerancePx);
    double minX = mapCoordinates.x - mapTolerance.x;
    double maxX = mapCoordinates.x + mapTolerance.x;
    double minY = mapCoordinates.y - mapTolerance.y;
    double maxY = mapCoordinates.y + mapTolerance.y;
    Envelope mapEnv(minX, minY, maxX, maxY);

    OGRPoint coordinates;
    PointId id = getLineStringMedianPointId(*line, mapEnv, &coordinates);

    if(!id) {
        return false;
    }

    int addedPtId = addPoint(line, id.pointId(), coordinates);
    saveToHistory();

    m_selectedPointId.setPointId(addedPtId); // Update only pointId.
    m_selectedPointCoordinates = coordinates;
    return true;
}

bool EditLayerOverlay::hasSelectedPoint(const OGRRawPoint* mapCoordinates) const
{
    bool ret = static_cast<bool>(m_selectedPointId);
    if(ret && mapCoordinates) {
        OGRRawPoint mapTolerance =
                m_map->getMapDistance(m_tolerancePx, m_tolerancePx);

        double minX = mapCoordinates->x - mapTolerance.x;
        double maxX = mapCoordinates->x + mapTolerance.x;
        double minY = mapCoordinates->y - mapTolerance.y;
        double maxY = mapCoordinates->y + mapTolerance.y;
        Envelope mapEnv(minX, minY, maxX, maxY);

        ret = geometryIntersects(m_selectedPointCoordinates, mapEnv);
    }
    return ret;
}

bool EditLayerOverlay::shiftPoint(const OGRRawPoint& mapOffset)
{
    if(!m_geometry || !m_selectedPointId) {
        return false;
    }

    return shiftGeometryPoint(*m_geometry, m_selectedPointId, mapOffset,
            &m_selectedPointCoordinates);
}

void EditLayerOverlay::freeResources()
{
    clearHistory();
    m_editedLayer.reset();
    m_datasource.reset();
    m_editedFeatureId = NOT_FOUND;
    m_geometry.reset();
}

//------------------------------------------------------------------------------
// PointId
//------------------------------------------------------------------------------

bool PointId::operator==(const PointId& other) const
{
    return m_pointId == other.m_pointId && m_ringId == other.m_ringId
            && m_geometryId == other.m_geometryId;
}

PointId getPointId(
        const OGRPoint& pt, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(pt, env)) {
        return PointId();
    }

    if(coordinates) {
        coordinates->setX(pt.getX());
        coordinates->setY(pt.getY());
    }
    return PointId(0);
}

PointId getLineStringPointId(
        const OGRLineString& line, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(line, env)) {
        return PointId();
    }

    int id = 0;
    bool found = false;
    OGRPointIterator* it = line.getPointIterator();
    OGRPoint pt;
    while(it->getNextPoint(&pt)) {
        if(geometryIntersects(pt, env)) {
            found = true;
            break;
        }
        ++id;
    }
    OGRPointIterator::destroy(it);

    if(found) {
        if(coordinates) {
            coordinates->setX(pt.getX());
            coordinates->setY(pt.getY());
        }
        return PointId(id);
    } else {
        return PointId();
    }
}

PointId getLineStringMedianPointId(
        const OGRLineString& line, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(line, env)) {
        return PointId();
    }

    int id = 0;
    bool found = false;

    OGRPoint medianPt;

    for(int i = 0, num = line.getNumPoints(); i < num - 1; ++i) {
        OGRPoint pt1;
        line.getPoint(i, &pt1);
        OGRPoint pt2;
        line.getPoint(i + 1, &pt2);
        medianPt = OGRPoint((pt2.getX() - pt1.getX()) / 2 + pt1.getX(),
                (pt2.getY() - pt1.getY()) / 2 + pt1.getY());
        if(geometryIntersects(medianPt, env)) {
            found = true;
            break;
        }
        ++id;
    }

    if(found) {
        if(coordinates) {
            coordinates->setX(medianPt.getX());
            coordinates->setY(medianPt.getY());
        }
        return PointId(id);
    } else {
        return PointId();
    }
}

PointId getPolygonPointId(
        const OGRPolygon& polygon, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(polygon, env)) {
        return PointId();
    }

    const OGRLinearRing* ring = polygon.getExteriorRing();
    int numInteriorRings = polygon.getNumInteriorRings();
    int ringId = 0;

    while(true) {
        if(!ring) {
            return PointId();
        }

        PointId id = getLineStringPointId(*ring, env, coordinates);
        if(id) {
            return PointId(id.pointId(), ringId);
        }

        if(ringId >= numInteriorRings) {
            break;
        }

        ring = polygon.getInteriorRing(ringId++);
    }

    return PointId();
}

PointId getMultiPointPointId(
        const OGRMultiPoint& mpt, const Envelope env, OGRPoint* coordinates)
{
    if(!geometryIntersects(mpt, env)) {
        return PointId();
    }

    for(int geometryId = 0, num = mpt.getNumGeometries(); geometryId < num;
            ++geometryId) {
        const OGRPoint* pt =
                static_cast<const OGRPoint*>(mpt.getGeometryRef(geometryId));
        if(geometryIntersects(*pt, env)) {
            if(coordinates) {
                coordinates->setX(pt->getX());
                coordinates->setY(pt->getY());
            }
            return PointId(0, NOT_FOUND, geometryId);
        }
    }

    return PointId();
}

PointId getMultiLineStringPointId(const OGRMultiLineString& mline,
        const Envelope env,
        OGRPoint* coordinates)
{
    if(!geometryIntersects(mline, env)) {
        return PointId();
    }

    for(int geometryId = 0, numGeoms = mline.getNumGeometries();
            geometryId < numGeoms; ++geometryId) {

        const OGRLineString* line = static_cast<const OGRLineString*>(
                mline.getGeometryRef(geometryId));

        PointId id = getLineStringPointId(*line, env, coordinates);
        if(id) {
            return PointId(id.pointId(), NOT_FOUND, geometryId);
        }
    }

    return PointId();
}

PointId getMultiPolygonPointId(const OGRMultiPolygon& mpolygon,
        const Envelope env,
        OGRPoint* coordinates)
{
    if(!geometryIntersects(mpolygon, env)) {
        return PointId();
    }

    for(int geometryId = 0, numGeoms = mpolygon.getNumGeometries();
            geometryId < numGeoms; ++geometryId) {

        const OGRPolygon* polygon = static_cast<const OGRPolygon*>(
                mpolygon.getGeometryRef(geometryId));
        PointId id = getPolygonPointId(*polygon, env, coordinates);
        if(id) {
            return PointId(id.pointId(), id.ringId(), geometryId);
        }
    }

    return PointId();
}

PointId getGeometryPointId(
        const OGRGeometry& geometry, const Envelope env, OGRPoint* coordinates)
{
    switch(OGR_GT_Flatten(geometry.getGeometryType())) {
        case wkbPoint: {
            const OGRPoint& pt = static_cast<const OGRPoint&>(geometry);
            return getPointId(pt, env, coordinates);
        }
        case wkbLineString: {
            const OGRLineString& lineString =
                    static_cast<const OGRLineString&>(geometry);
            return getLineStringPointId(lineString, env, coordinates);
        }
        case wkbPolygon: {
            const OGRPolygon& polygon =
                    static_cast<const OGRPolygon&>(geometry);
            return getPolygonPointId(polygon, env, coordinates);
        }
        case wkbMultiPoint: {
            const OGRMultiPoint& mpt =
                    static_cast<const OGRMultiPoint&>(geometry);
            return getMultiPointPointId(mpt, env, coordinates);
        }
        case wkbMultiLineString: {
            const OGRMultiLineString& mline =
                    static_cast<const OGRMultiLineString&>(geometry);
            return getMultiLineStringPointId(mline, env, coordinates);
        }
        case wkbMultiPolygon: {
            const OGRMultiPolygon& mpolygon =
                    static_cast<const OGRMultiPolygon&>(geometry);
            return getMultiPolygonPointId(mpolygon, env, coordinates);
        }
    }
    return PointId();
}

//------------------------------------------------------------------------------
// Static functions
//------------------------------------------------------------------------------

bool shiftPoint(OGRPoint& pt,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    if(0 != id.pointId()) {
        return false;
    }

    pt.setX(pt.getX() + offset.x);
    pt.setY(pt.getY() + offset.y);
    if(coordinates) {
        coordinates->setX(pt.getX());
        coordinates->setY(pt.getY());
    }
    return true;
}

bool shiftLineStringPoint(OGRLineString& lineString,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    if(pointId < 0 || pointId >= lineString.getNumPoints()) {
        return false;
    }

    OGRPoint pt;
    lineString.getPoint(pointId, &pt);
    lineString.setPoint(pointId, pt.getX() + offset.x, pt.getY() + offset.y);
    if(coordinates) {
        coordinates->setX(pt.getX());
        coordinates->setY(pt.getY());
    }
    return true;
}

bool shiftPolygonPoint(OGRPolygon& polygon,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    int ringId = id.ringId();
    int numInteriorRings = polygon.getNumInteriorRings();

    // ringId == 0 - exterior ring, 1+ - interior rings
    if(pointId < 0 || ringId < 0 || ringId > numInteriorRings) {
        return false;
    }

    OGRLinearRing* ring = (0 == ringId) ? polygon.getExteriorRing()
                                        : polygon.getInteriorRing(ringId - 1);
    if(!ring) {
        return false;
    }

    int ringNumPoints = ring->getNumPoints();
    if(pointId >= ringNumPoints) {
        return false;
    }

    return shiftLineStringPoint(*ring, PointId(pointId), offset, coordinates);
}

bool shiftMultiPointPoint(OGRMultiPoint& mpt,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    int geometryId = id.geometryId();
    if(pointId != 0 || geometryId < 0 || geometryId >= mpt.getNumGeometries()) {
        return false;
    }
    OGRPoint* pt = static_cast<OGRPoint*>(mpt.getGeometryRef(geometryId));
    return shiftPoint(*pt, PointId(0), offset, coordinates);
}

bool shiftMultiLineStringPoint(OGRMultiLineString& mline,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    int geometryId = id.geometryId();
    if(pointId < 0 || geometryId < 0
            || geometryId >= mline.getNumGeometries()) {
        return false;
    }

    OGRLineString* line =
            static_cast<OGRLineString*>(mline.getGeometryRef(geometryId));

    int lineNumPoints = line->getNumPoints();
    if(pointId >= lineNumPoints) {
        return false;
    }

    return shiftLineStringPoint(*line, PointId(pointId), offset, coordinates);
}

bool shiftMultiPolygonPoint(OGRMultiPolygon& mpolygon,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    int pointId = id.pointId();
    int ringId = id.ringId();
    int geometryId = id.geometryId();
    if(pointId < 0 || ringId < 0 || geometryId < 0
            || geometryId >= mpolygon.getNumGeometries()) {
        return false;
    }

    OGRPolygon* polygon =
            static_cast<OGRPolygon*>(mpolygon.getGeometryRef(geometryId));

    return shiftPolygonPoint(
            *polygon, PointId(pointId, ringId), offset, coordinates);
}

bool shiftGeometryPoint(OGRGeometry& geometry,
        const PointId& id,
        const OGRRawPoint& offset,
        OGRPoint* coordinates)
{
    switch(OGR_GT_Flatten(geometry.getGeometryType())) {
        case wkbPoint: {
            OGRPoint& pt = static_cast<OGRPoint&>(geometry);
            return shiftPoint(pt, id, offset, coordinates);
        }
        case wkbLineString: {
            OGRLineString& lineString = static_cast<OGRLineString&>(geometry);
            return shiftLineStringPoint(lineString, id, offset, coordinates);
        }
        case wkbPolygon: {
            OGRPolygon& polygon = static_cast<OGRPolygon&>(geometry);
            return shiftPolygonPoint(polygon, id, offset, coordinates);
        }
        case wkbMultiPoint: {
            OGRMultiPoint& mpt = static_cast<OGRMultiPoint&>(geometry);
            return shiftMultiPointPoint(mpt, id, offset, coordinates);
        }
        case wkbMultiLineString: {
            OGRMultiLineString& mline =
                    static_cast<OGRMultiLineString&>(geometry);
            return shiftMultiLineStringPoint(mline, id, offset, coordinates);
        }
        case wkbMultiPolygon: {
            OGRMultiPolygon& mpolygon = static_cast<OGRMultiPolygon&>(geometry);
            return shiftMultiPolygonPoint(mpolygon, id, offset, coordinates);
        }
        default: {
            return false;
        }
    }
}

} // namespace ngs
