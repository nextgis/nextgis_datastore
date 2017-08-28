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

    virtual bool fill(bool isLastTry) = 0;
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
        geometries = 0,
        selectedGeometry,
        lines,
        selectedLine,
        medianPoints,
        points,
        selectedPoint
    };

public:
    explicit GlEditLayerOverlay(const MapView& map);
    virtual ~GlEditLayerOverlay() = default;

    // Overlay interface
public:
    virtual void setVisible(bool visible) override;

    // EditLayerOverlay interface
public:
    virtual void setGeometry(GeometryUPtr geometry) override;
    virtual OGRGeometry* releaseGeometry() override;
    virtual void resetGeometry() override;
    virtual bool selectPoint(const OGRRawPoint& mapCoordinates) override;
    virtual bool shiftPoint(const OGRRawPoint& mapOffset) override;
    virtual bool addGeometryPart(const OGRRawPoint& geometryCenter) override;
    virtual bool deleteGeometryPart() override;
    virtual bool undo() override;
    virtual bool redo() override;

    // GlRenderOverlay interface
public:
    virtual bool fill(bool isLastTry) override;
    virtual bool draw() override;

protected:
    void fillPoint();
    void fillLine();
    void freeResource(OverlayElement& element);
    void freeResources();

private:
    std::map<ElementType, OverlayElement> m_elements;
};

} // namespace ngs

#endif // NGSGLOVERLAY_H
