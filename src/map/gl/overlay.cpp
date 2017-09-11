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

// gdal
#include "ogr_core.h"

#include "map/gl/view.h"
#include "map/mapview.h"


namespace ngs {

//------------------------------------------------------------------------------
// GlRenderOverlay
//------------------------------------------------------------------------------

GlRenderOverlay::GlRenderOverlay()
{
}

//------------------------------------------------------------------------------
// GlEditLayerOverlay
//------------------------------------------------------------------------------

// TODO: set colors
constexpr ngsRGBA geometryColor = {0, 0, 255, 255};
constexpr ngsRGBA selectedGeometryColor = {0, 0, 255, 255};
constexpr ngsRGBA lineColor = {0, 0, 255, 255};
constexpr ngsRGBA selectedLineColor = {0, 0, 255, 255};
constexpr ngsRGBA medianPointColor = {0, 0, 255, 255};
constexpr ngsRGBA pointColor = {0, 0, 255, 255};
constexpr ngsRGBA selectedPointColor = {255, 0, 0, 255};

GlEditLayerOverlay::GlEditLayerOverlay(MapView* map) : EditLayerOverlay(map),
    GlRenderOverlay()
{
    GlView* mapView = dynamic_cast<GlView*>(m_map);
    const TextureAtlas* atlas = mapView ? mapView->textureAtlas() : nullptr;

    OverlayElement points;
    points.m_style = StylePtr(Style::createStyle("simplePoint", atlas));
    SimplePointStyle* style = ngsDynamicCast(SimplePointStyle, points.m_style);
    style->setColor(pointColor);
    m_elements[EET_POINT] = points;

    OverlayElement selectedPoint;
    selectedPoint.m_style = StylePtr(Style::createStyle("simplePoint", atlas));
    style = ngsDynamicCast(SimplePointStyle, selectedPoint.m_style);
    style->setColor(selectedPointColor);
    m_elements[EET_SELECTED_POINT] = selectedPoint;
}

bool GlEditLayerOverlay::setStyleName(
        enum ngsEditElementType type, const char* name)
{
    GlView* mapView = dynamic_cast<GlView*>(m_map);
    Style* style = static_cast<Style*>(Style::createStyle(
            name, mapView ? mapView->textureAtlas() : nullptr));
    if(nullptr == style)
        return false;

    auto it = m_elements.find(type);
    if(m_elements.end() != it)
        freeGlStyle(it->second);

    OverlayElement element;
    element.m_style = StylePtr(style);
    m_elements[type] = element;
    return true;
}

bool GlEditLayerOverlay::setStyle(
        enum ngsEditElementType type, const CPLJSONObject& jsonStyle)
{
    auto it = m_elements.find(type);
    if(m_elements.end() == it || !it->second.m_style)
        return false;
    return it->second.m_style->load(jsonStyle);
}

CPLJSONObject GlEditLayerOverlay::style(enum ngsEditElementType type) const
{
    auto it = m_elements.find(type);
    if(m_elements.end() == it || !it->second.m_style)
        return CPLJSONObject();
    return it->second.m_style->save();
}

void GlEditLayerOverlay::setVisible(bool visible)
{
    EditLayerOverlay::setVisible(visible);
    if(!visible) {
        freeGlBuffers();
    }
}

bool GlEditLayerOverlay::undo()
{
    bool ret = EditLayerOverlay::undo();
    if(ret) {
        fill();
    }
    return ret;
}

bool GlEditLayerOverlay::redo()
{
    bool ret = EditLayerOverlay::redo();
    if(ret) {
        fill();
    }
    return ret;
}

bool GlEditLayerOverlay::addGeometryPart()
{
    bool ret = EditLayerOverlay::addGeometryPart();
    if(ret) {
        fill();
    }
    return ret;
}

bool GlEditLayerOverlay::deleteGeometryPart()
{
    bool ret = EditLayerOverlay::deleteGeometryPart();
    if(ret) {
        fill();
    }
    return ret;
}

void GlEditLayerOverlay::setGeometry(GeometryUPtr geometry)
{
    EditLayerOverlay::setGeometry(std::move(geometry));
    freeGlBuffers();

    if(!m_geometry) {
        return;
    }
    fill();
}

bool GlEditLayerOverlay::selectPoint(const OGRRawPoint& mapCoordinates)
{
    bool ret = EditLayerOverlay::selectPoint(mapCoordinates);
    if(ret) {
        fill();
    }
    return ret;
}

bool GlEditLayerOverlay::shiftPoint(const OGRRawPoint& mapOffset)
{
    bool ret = EditLayerOverlay::shiftPoint(mapOffset);
    if(ret) {
        fill();
    }
    return ret;
}

bool GlEditLayerOverlay::fill()
{
    if(!m_geometry) {
        return false;
    }

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbPoint:
        case wkbMultiPoint: {
            fillPoint();
            break;
        }

        case wkbLineString:
        case wkbMultiLineString: {
            fillLine();
            break;
        }
        default:
            break; // Not supported yet
    }
    return true;
}

void GlEditLayerOverlay::fillPoint()
{
    if(!m_geometry) {
        return;
    }

    OGRwkbGeometryType type = OGR_GT_Flatten(m_geometry->getGeometryType());
    if(wkbPoint != type && wkbMultiPoint != type) {
        return;
    }

    switch(type) {
        case wkbPoint: {
            freeGlBuffer(m_elements.at(EET_POINT));
            freeGlBuffer(m_elements.at(EET_SELECTED_POINT));

            bool isSelectedPointType = (PointId(0) == m_selectedPointId);
            MarkerStyle* style;
            if(isSelectedPointType) {
                style = ngsDynamicCast(
                        MarkerStyle, m_elements.at(EET_SELECTED_POINT).m_style);
            } else {
                style = ngsDynamicCast(
                        MarkerStyle, m_elements.at(EET_POINT).m_style);
            }

            const OGRPoint* pt = static_cast<const OGRPoint*>(m_geometry.get());

            GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
            SimplePoint spt;
            spt.x = pt->getX();
            spt.y = pt->getY();
            /*int index = */
            style->addPoint(spt, 0, buffer);

            VectorGlObject* bufferArray = new VectorGlObject();
            bufferArray->addBuffer(buffer);

            if(isSelectedPointType) {
                m_elements.at(EET_SELECTED_POINT).m_glBuffer =
                        GlObjectPtr(bufferArray);
            } else {
                m_elements.at(EET_POINT).m_glBuffer = GlObjectPtr(bufferArray);
            }
            break;
        }
        case wkbMultiPoint: {
            freeGlBuffer(m_elements.at(EET_POINT));
            freeGlBuffer(m_elements.at(EET_SELECTED_POINT));

            const OGRMultiPoint* mpt =
                    static_cast<const OGRMultiPoint*>(m_geometry.get());

            GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
            VectorGlObject* bufferArray = new VectorGlObject();

            int index = 0;
            for(int i = 0, num = mpt->getNumGeometries(); i < num; ++i) {
                const OGRPoint* pt =
                        static_cast<const OGRPoint*>(mpt->getGeometryRef(i));

                if(PointId(0, NOT_FOUND, i) == m_selectedPointId) {
                    GlBuffer* selBuffer = new GlBuffer(GlBuffer::BF_PT);
                    PointStyle* selStyle = ngsDynamicCast(PointStyle,
                            m_elements.at(EET_SELECTED_POINT).m_style);
                    SimplePoint spt;
                    spt.x = pt->getX();
                    spt.y = pt->getY();
                    /*int selIndex = */
                    selStyle->addPoint(spt, 0, selBuffer);

                    VectorGlObject* selBufferArray = new VectorGlObject();
                    selBufferArray->addBuffer(selBuffer);

                    m_elements.at(EET_SELECTED_POINT).m_glBuffer =
                            GlObjectPtr(selBufferArray);
                    continue;
                }

                if(buffer->vertexSize() >= GlBuffer::maxVertices()) {
                    bufferArray->addBuffer(buffer);
                    index = 0;
                    buffer = new GlBuffer(GlBuffer::BF_PT);
                }

                PointStyle* style = ngsDynamicCast(
                        PointStyle, m_elements.at(EET_POINT).m_style);
                SimplePoint spt;
                spt.x = pt->getX();
                spt.y = pt->getY();
                index = style->addPoint(spt, index, buffer);

            }

            bufferArray->addBuffer(buffer);
            m_elements.at(EET_POINT).m_glBuffer = GlObjectPtr(bufferArray);
            break;
        }
    }
}

void GlEditLayerOverlay::fillLine()
{
    // TODO
    //OGRLineString* line = static_cast<OGRLineString*>(m_geometry.get());

    //GlBuffer* buffer = new GlBuffer(GlBuffer::BF_LINE);
    //VectorGlObject* bufferArray = new VectorGlObject();
    //bufferArray->addBuffer(buffer);

    //return bufferArray;
}

void GlEditLayerOverlay::freeResources()
{
    EditLayerOverlay::freeResources();
    freeGlBuffers();
    //freeGlStyles(); // TODO: only on close map
    //m_elements.clear(); // TODO: only on close map
}

void GlEditLayerOverlay::freeGlStyle(OverlayElement& element)
{
    if(element.m_style) {
        GlView* glView = dynamic_cast<GlView*>(m_map);
        if(glView) {
            glView->freeResource(element.m_style);
            element.m_style = nullptr;
        }
    }
}

void GlEditLayerOverlay::freeGlStyles()
{
    for(auto it = m_elements.begin(); it != m_elements.end(); ++it) {
        freeGlStyle(it->second);
    }
}

void GlEditLayerOverlay::freeGlBuffer(OverlayElement& element)
{
    if(element.m_glBuffer) {
        GlView* glView = dynamic_cast<GlView*>(m_map);
        if(glView) {
            glView->freeResource(element.m_glBuffer);
            element.m_glBuffer = nullptr;
        }
    }
}

void GlEditLayerOverlay::freeGlBuffers()
{
    for(auto it = m_elements.begin(); it != m_elements.end(); ++it) {
        freeGlBuffer(it->second);
    }
}

bool GlEditLayerOverlay::draw()
{
    if(!visible() || !m_elements.size()) {
        // !m_elements.size() should never happen.
        return true;
    }

    if(!m_elements.at(EET_SELECTED_POINT).m_glBuffer) {
        // One of the vertices must always be selected.
        return false; // Data is not yet loaded.
    }

    for(auto it = m_elements.begin(); it != m_elements.end(); ++it) {
        GlObjectPtr glBuffer = it->second.m_glBuffer;
        if(!glBuffer) {
            continue;
        }

        StylePtr style = it->second.m_style;

        VectorGlObject* vectorGlBuffer =
                ngsDynamicCast(VectorGlObject, glBuffer);
        for(const GlBufferPtr& buff : vectorGlBuffer->buffers()) {

            if(buff->bound()) {
                buff->rebind();
            } else {
                buff->bind();
            }

            style->prepare(m_map->getSceneMatrix(), m_map->getInvViewMatrix(),
                    buff->type());
            style->draw(*buff);
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// GlLocationOverlay
//------------------------------------------------------------------------------

GlLocationOverlay::GlLocationOverlay(MapView* map) : LocationOverlay(map),
    GlRenderOverlay(),
    m_style(static_cast<PointStyle*>(Style::createStyle("simpleLocation", nullptr)))
{
    m_style->setType(PT_DIAMOND);
    m_style->setColor({255,0,0,255});
}

bool GlLocationOverlay::setStyleName(const char* name)
{
    if(EQUAL(name, m_style->name())) {
        return true;
    }

    GlView* mapView = dynamic_cast<GlView*>(m_map);
    PointStyle* style = static_cast<PointStyle*>(
                Style::createStyle(name, mapView ? mapView->textureAtlas() : nullptr));
    if(nullptr == style) {
        return false;
    }
    if(mapView) {
        mapView->freeResource(m_style);
    }
    m_style = PointStylePtr(style);

    return true;
}

bool GlLocationOverlay::setStyle(const CPLJSONObject& style)
{
    return m_style->load(style);
}

bool GlLocationOverlay::draw()
{
    if(!visible()) {
        return true;
    }

    GlBuffer buffer(GlBuffer::BF_FILL);
    m_style->setRotation(m_direction);
    /*int index = */m_style->addPoint(m_location, 0, &buffer);
    LocationStyle* lStyle = ngsDynamicCast(LocationStyle, m_style);
    if(nullptr != lStyle) {
        lStyle->setStatus(isEqual(m_direction, -1.0f) ? LocationStyle::LS_STAY :
                                                        LocationStyle::LS_MOVE);
    }

    buffer.bind();
    m_style->prepare(m_map->getSceneMatrix(), m_map->getInvViewMatrix(),
                     buffer.type());
    m_style->draw(buffer);
    buffer.destroy();

    return true;
}

} // namespace ngs
