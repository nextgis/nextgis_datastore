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
#ifndef NGSMAPVIEW_H
#define NGSMAPVIEW_H

#include "map.h"
#include "maptransform.h"

#include "ogr_geometry.h"

#include "overlay.h"

namespace ngs {

/**
 * @brief The MapView class Base class for map with render support
 */
class MapView : public Map, public MapTransform
{
public:
    MapView();
    explicit MapView(const CPLString& name, const CPLString& description,
            unsigned short epsg, const Envelope& bounds);
    virtual ~MapView() = default;
    virtual bool draw(enum ngsDrawState state, const Progress& progress = Progress());

    size_t overlayCount() const { return m_overlays.size(); }
    OverlayPtr getOverlay(enum ngsMapOverlyType type) const;
    void setOverlayVisible(enum ngsMapOverlyType typeMask, bool visible);
    ngsDrawState mapTouch(double x, double y, enum ngsMapTouchType type);

    // Map interface
protected:
    virtual bool openInternal(const CPLJSONObject& root, MapFile * const mapFile) override;
    virtual bool saveInternal(CPLJSONObject &root, MapFile * const mapFile) override;

protected:
    virtual void clearBackground() = 0;
    virtual void createOverlays() = 0;

protected:
    std::vector<OverlayPtr> m_overlays;
    OGRRawPoint m_touchStartPoint;
    bool m_touchMoved;
    bool m_touchSelectedPoint;
};

typedef std::shared_ptr<MapView> MapViewPtr;

/**
 * @brief The IRenderLayer class Interface for renderable map layers
 */
class IRenderLayer
{
public:
    virtual ~IRenderLayer() = default;
    virtual double draw(enum ngsDrawState state, MapView* map, float level,
                        const Progress& progress = Progress()) = 0;
};

class IOverlay
{
public:
    virtual ~IOverlay() = default;
    virtual double draw(enum ngsDrawState state, MapView* map, float level,
                        const Progress& progress = Progress()) = 0;
};

}
#endif // NGSMAPVIEW_H
