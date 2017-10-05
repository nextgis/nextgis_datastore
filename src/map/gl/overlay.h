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

#ifndef NGSGLOVERLAY_H
#define NGSGLOVERLAY_H

// stl
#include <functional>
#include <map>
#include <memory>

#include "layer.h"
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
    explicit GlEditLayerOverlay(MapView* map);
    virtual ~GlEditLayerOverlay() = default;

    bool setStyleName(enum ngsEditStyleType type, const char* name);
    bool setStyle(enum ngsEditStyleType type, const CPLJSONObject& jsonStyle);
    CPLJSONObject style(enum ngsEditStyleType type) const;

    // Overlay interface
public:
    virtual void setVisible(bool visible) override;

    // EditLayerOverlay interface
public:
    virtual bool undo() override;
    virtual bool redo() override;

    virtual bool addPoint(OGRPoint* coordinates = nullptr) override;
    virtual enum ngsEditDeleteType deletePoint() override;
    virtual bool addHole() override;
    virtual enum ngsEditDeleteType deleteHole() override;
    virtual bool addGeometryPart() override;
    virtual enum ngsEditDeleteType deleteGeometryPart() override;

protected:
    virtual bool singleTap(const OGRRawPoint& mapCoordinates) override;
    virtual bool shiftPoint(const OGRRawPoint& mapOffset) override;
    virtual void setGeometry(GeometryUPtr geometry) override;
    virtual void freeResources() override;

    // GlRenderOverlay interface
public:
    virtual bool fill() override;
    virtual bool draw() override;

protected:
    void fillPoints();
    void fillLines();
    void fillPolygons();
    void freeGlStyle(StylePtr style);
    void freeGlBuffer(GlObjectPtr buffer);
    void freeGlBuffers();

private:
    using GetPointFunc = std::function<SimplePoint(int index)>;
    using GetLineFunc = std::function<const OGRLineString*(int index)>;
    using GetPolygonFunc = std::function<const OGRPolygon*(int index)>;
    using IsSelectedGeometryFunc = std::function<bool(int index)>;

    void fillPointElements(int numPoints,
            GetPointFunc getPointFunc,
            IsSelectedGeometryFunc isSelectedPointFunc);
    void fillMedianPointElements(int numPoints,
            GetPointFunc getPointFunc,
            IsSelectedGeometryFunc isSelectedMedianPointFunc);
    void fillLineElements(int numLines,
            GetLineFunc getLineFunc,
            IsSelectedGeometryFunc isSelectedLineFunc,
            bool addToBuffer = false);
    void fillLineBuffers(
            const OGRLineString* line, VectorGlObject* bufferArray);
    void fillPolygonElements(int numPolygons,
            GetPolygonFunc getPolygonFunc,
            IsSelectedGeometryFunc isSelectedPolygonFunc);
    void fillPolygonBuffers(
            const OGRPolygon* polygon, VectorGlObject* bufferArray);
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
    explicit GlLocationOverlay(MapView* map);
    virtual ~GlLocationOverlay() = default;

    bool setStyleName(const char* name);
    bool setStyle(const CPLJSONObject& style);
    CPLJSONObject style() const { return m_style->save(); }

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
