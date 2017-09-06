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
#include <map>
#include <memory>

#include "layer.h"
#include "map/overlay.h"

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
    struct OverlayElement
    {
        GlObjectPtr m_glBuffer;
        StylePtr m_style;
    };

    enum class ElementType
    {
        GEOMETRIES = 0,
        SELECTED_GEOMETRY,
        LINES,
        SELECTED_LINE,
        MEDIAN_POINTS,
        POINTS,
        SELECTED_POINT
    };

public:
    explicit GlEditLayerOverlay(MapView* map);
    virtual ~GlEditLayerOverlay() = default;

    // Overlay interface
public:
    virtual void setVisible(bool visible) override;

    // EditLayerOverlay interface
public:
    virtual bool undo() override;
    virtual bool redo() override;

    virtual bool addGeometryPart() override;
    virtual bool deleteGeometryPart() override;

    virtual bool selectPoint(const OGRRawPoint& mapCoordinates) override;
    virtual bool shiftPoint(const OGRRawPoint& mapOffset) override;

protected:
    virtual void setGeometry(GeometryUPtr geometry) override;
    virtual void freeResources() override;

    // GlRenderOverlay interface
public:
    virtual bool fill() override;
    virtual bool draw() override;

protected:
    void fillPoint();
    void fillLine();
    void freeGlBuffer(OverlayElement& element);
    void freeGlBuffers();

private:
    std::map<ElementType, OverlayElement> m_elements;
};

class GlLocationOverlay : public LocationOverlay, public GlRenderOverlay
{
public:
    explicit GlLocationOverlay(MapView* map);
    virtual ~GlLocationOverlay() = default;

    bool setStyleName(const char* name);
    bool setStyle(const CPLJSONObject& style);

    // GlRenderOverlay interface
public:
    virtual bool fill() override { return true; }
    virtual bool draw() override;

private:
    PointStylePtr m_style;
};

} // namespace ngs

#endif // NGSGLOVERLAY_H
