/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
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
#include "mapview.h"

#include "api_priv.h"
#include "catalog/mapfile.h"
#include "ngstore/util/constants.h"

namespace ngs {

constexpr const char* MAP_EXTENT_KEY = "extent";
constexpr const char* MAP_ROTATE_X_KEY = "rotate_x";
constexpr const char* MAP_ROTATE_Y_KEY = "rotate_y";
constexpr const char* MAP_ROTATE_Z_KEY = "rotate_z";
constexpr const char* MAP_X_LOOP_KEY = "x_looped";

MapView::MapView()
        : Map()
        , MapTransform(480, 640)
        , m_touchMoved(false)
        , m_touchSelectedPoint(false)
{
}

MapView::MapView(const CPLString& name,
        const CPLString& description,
        unsigned short epsg,
        const Envelope& bounds)
        : Map(name, description, epsg, bounds)
        , MapTransform(480, 640)
        , m_touchMoved(false)
        , m_touchSelectedPoint(false)
{
}

bool MapView::draw(ngsDrawState state, const Progress &progress)
{
    clearBackground();

    if(m_layers.empty()) {
        progress.onProgress(COD_FINISHED, 1.0,
                            _("No layers. Nothing to render."));
        return true;
    }

    float level = 0.0f;
    double done = 0.0;
    for(auto it = m_layers.rbegin(); it != m_layers.rend(); ++it) {
        LayerPtr layer = *it;
        IRenderLayer* const renderLayer = ngsDynamicCast(IRenderLayer, layer);
        done += renderLayer->draw(state, this, level++, progress);
    }
    for (auto it = m_overlays.rbegin(); it != m_overlays.rend(); ++it) {
        OverlayPtr overlay = *it;
        IOverlay* const iOverlay = ngsDynamicCast(IOverlay, overlay);
        done += iOverlay->draw(state, this, level++, progress);
    }

    size_t size = m_layers.size() + m_overlays.size();
    if (isEqual(done, size)) {
        progress.onProgress(COD_FINISHED, 1.0, _("Map render finished."));
    } else {
        progress.onProgress(COD_IN_PROCESS, done / (size), _("Rendering ..."));
    }

    return true;
}

bool MapView::openInternal(const CPLJSONObject &root, MapFile * const mapFile)
{
    if(!Map::openInternal(root, mapFile))
        return false;

    setRotate(DIR_X, root.GetDouble(MAP_ROTATE_X_KEY, 0));
    setRotate(DIR_Y, root.GetDouble(MAP_ROTATE_Y_KEY, 0));
    setRotate(DIR_Z, root.GetDouble(MAP_ROTATE_Z_KEY, 0));

    Envelope env;
    env.load(root.GetObject(MAP_EXTENT_KEY), DEFAULT_BOUNDS);
    setExtent(env);

    m_XAxisLooped = root.GetBool(MAP_X_LOOP_KEY, true);

    return true;
}

bool MapView::saveInternal(CPLJSONObject &root, MapFile * const mapFile)
{
    if(!Map::saveInternal(root, mapFile))
        return false;

    root.Add(MAP_EXTENT_KEY, getExtent().save());
    root.Add(MAP_ROTATE_X_KEY, getRotate(DIR_X));
    root.Add(MAP_ROTATE_Y_KEY, getRotate(DIR_Y));
    root.Add(MAP_ROTATE_Z_KEY, getRotate(DIR_Z));

    root.Add(MAP_X_LOOP_KEY, m_XAxisLooped);
    return true;
}

OverlayPtr MapView::getOverlay(ngsMapOverlyType type) const
{
    int index = Overlay::getOverlayIndexFromType(type);
    if (-1 == index)
        return nullptr;

    size_t overlayIndex = static_cast<size_t>(index);
    if (overlayIndex >= m_overlays.size())
        return nullptr;

    return m_overlays[overlayIndex];
}

void MapView::setOverlayVisible(enum ngsMapOverlyType typeMask, bool visible)
{
    OverlayPtr overlay;

    // TODO:
    //if (MOT_LOCATION & typeMask) {
    //    overlay = getOverlay(MOT_LOCATION);
    //    overlay->setVisible(visible);
    //}
    //if (MOT_TRACK & typeMask) {
    //    overlay = getOverlay(MOT_TRACK);
    //    overlay->setVisible(visible);
    //}
    if (MOT_EDIT & typeMask) {
        overlay = getOverlay(MOT_EDIT);
        overlay->setVisible(visible);
    }
}

ngsDrawState MapView::mapTouch(double x, double y, enum ngsMapTouchType type)
{
    EditLayerOverlay* editOverlay = nullptr;
    OverlayPtr overlay = getOverlay(MOT_EDIT);
    if (overlay) {
        editOverlay = ngsDynamicCast(EditLayerOverlay, overlay);
    }
    bool editMode = editOverlay && editOverlay->visible();

    switch(type) {
        case MTT_ON_DOWN: {
            m_touchStartPoint = OGRRawPoint(x, y);
            if (editMode) {
                OGRRawPoint mapPt = displayToWorld(
                        OGRRawPoint(m_touchStartPoint.x, m_touchStartPoint.y));
                if (!getYAxisInverted()) {
                    mapPt.y = -mapPt.y;
                }
                m_touchSelectedPoint = editOverlay->hasSelectedPoint(&mapPt);
            }
            return DS_NOTHING;
        }
        case MTT_ON_MOVE: {
            if (!m_touchMoved) {
                m_touchMoved = true;
            }

            OGRRawPoint pt = OGRRawPoint(x, y);
            OGRRawPoint offset(
                    pt.x - m_touchStartPoint.x, pt.y - m_touchStartPoint.y);
            OGRRawPoint mapOffset = getMapDistance(offset.x, offset.y);
            if (!getYAxisInverted()) {
                mapOffset.y = -mapOffset.y;
            }

            bool moveMap = true;
            if (editMode && m_touchSelectedPoint) {
                moveMap = !editOverlay->shiftPoint(mapOffset);
            }

            if (moveMap) {
                OGRRawPoint mapCenter = getCenter();
                setCenter(mapCenter.x - mapOffset.x, mapCenter.y - mapOffset.y);
            }

            m_touchStartPoint = pt;
            return DS_PRESERVED;
        }
        case MTT_ON_UP: {
            if (m_touchMoved) {
                m_touchMoved = false;
                bool pointWasMoved = m_touchSelectedPoint;
                if (m_touchSelectedPoint) {
                    m_touchSelectedPoint = false;
                }
                if (editMode && pointWasMoved) {
                    editOverlay->saveToHistory();
                } else { // if normal mode
                    return DS_NORMAL;
                }
            } else if(editMode) {
                OGRRawPoint mapPt = displayToWorld(
                        OGRRawPoint(m_touchStartPoint.x, m_touchStartPoint.y));
                if(!getYAxisInverted()) {
                    mapPt.y = -mapPt.y;
                }
                if(editOverlay->selectPoint(mapPt)) {
                    return DS_PRESERVED;
                }
            }
            return DS_NOTHING;
        }
//        default:
//            return DS_NOTHING;
    }
}

bool MapView::setOptions(const Options& options)
{
    double reduceFactor = options.doubleOption("VIEWPORT_REDUCE_FACTOR", 1.0);
    setReduceFactor(reduceFactor);

    char zoomIncrement = static_cast<char>(options.intOption("ZOOM_INCREMENT", 0));
    setZoomIncrement(zoomIncrement);
    return true;
}

} // namespace ngs
