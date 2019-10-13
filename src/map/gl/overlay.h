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

#ifndef NGSGLOVERLAY_H
#define NGSGLOVERLAY_H

#include "layer.h"

// stl
#include <functional>
#include <map>
#include <memory>

#include "map/overlay.h"
#include "ngstore/codes.h"

namespace ngs {

class GlRenderOverlay
{
public:
    GlRenderOverlay();
    virtual ~GlRenderOverlay() = default;

    virtual bool fill() = 0;
    virtual bool draw() = 0;
};

class GlEditLayerOverlay : public EditLayerOverlay, public GlRenderOverlay
{
public:
    explicit GlEditLayerOverlay(MapView *map);
    virtual ~GlEditLayerOverlay() override = default;

    bool setStyleName(enum ngsEditStyleType type, const std::string &name) override;
    bool setStyle(enum ngsEditStyleType type, const CPLJSONObject &jsonStyle) override;
    CPLJSONObject style(enum ngsEditStyleType type) const override;

    // Overlay interface
public:
    virtual void setVisible(bool visible) override;

    // EditLayerOverlay interface
public:
    virtual bool undo() override;
    virtual bool redo() override;

    virtual bool addPoint(double x, double y) override;
    virtual enum ngsEditDeleteResult deletePoint() override;
    virtual bool addHole() override;
    virtual enum ngsEditDeleteResult deleteHole() override;
    virtual bool addGeometryPart() override;
    virtual enum ngsEditDeleteResult deleteGeometryPart() override;
    virtual ngsPointId touch(double x, double y, enum ngsMapTouchType type) override;

    // GlRenderOverlay interface
public:
    virtual bool fill() override;
    virtual bool draw() override;

protected:
    void freeGlStyle(StylePtr style);
    void freeGlBuffer(GlObjectPtr buffer);
    void freeGlBuffers();

private:
    void fillPointElements(const std::vector<OGRRawPoint> &points, int selectedPointId);
    void fillMiddlePointElements(const std::vector<OGRRawPoint> &points);
    void fillLineElements(std::vector<Line> lines, int selectedLine,
                          int selectedPoint,
                          bool addToBuffer = false);
    void fillLineBuffers(const Line &lineline, VectorGlObject* bufferArray);
    void fillPolygonElements(std::vector<Polygon> polygons, int selectedPart,
                             int selectedRing, int selectedPoint);
    void fillPolygonBuffers(const Polygon &polygon, VectorGlObject *bufferArray);
    void fillCrossElement();

private:
    std::map<ngsEditElementType, GlObjectPtr> m_elements;
    PointStylePtr m_pointStyle;
    EditLineStylePtr m_lineStyle;
    EditFillStylePtr m_fillStyle;
    PointStylePtr m_crossStyle;
};

class GlLocationOverlay : public LocationOverlay, public GlRenderOverlay
{
public:
    explicit GlLocationOverlay(MapView *map);
    virtual ~GlLocationOverlay() override = default;

    virtual bool setStyleName(const std::string &name) override;
    virtual bool setStyle(const CPLJSONObject &style) override;
    virtual CPLJSONObject style() const override { return m_style->save(); }

    // GlRenderOverlay interface
public:
    virtual bool fill() override { return true; }
    virtual bool draw() override;

private:
    PointStylePtr m_style;
    unsigned short m_stayIndex, m_moveIndex;
};

} // namespace ngs

#endif // NGSGLOVERLAY_H
