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

GlEditLayerOverlay::GlEditLayerOverlay(const MapView& map) : EditLayerOverlay(map),
    GlRenderOverlay()
{
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

// TODO: set colors
constexpr ngsRGBA geometryColor = {0, 0, 255, 255};
constexpr ngsRGBA selectedGeometryColor = {0, 0, 255, 255};
constexpr ngsRGBA lineColor = {0, 0, 255, 255};
constexpr ngsRGBA selectedLineColor = {0, 0, 255, 255};
constexpr ngsRGBA medianPointColor = {0, 0, 255, 255};
constexpr ngsRGBA pointColor = {0, 0, 255, 255};
constexpr ngsRGBA selectedPointColor = {255, 0, 0, 255};

void GlEditLayerOverlay::setGeometry(GeometryUPtr geometry)
{
    EditLayerOverlay::setGeometry(std::move(geometry));
    freeGlBuffers();

    if(!m_geometry) {
        return;
    }

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbPoint:
        case wkbMultiPoint: {
            OverlayElement points;
            points.m_style = StylePtr(Style::createStyle("simplePoint"));
            SimpleVectorStyle* vectorStyle =
                    ngsDynamicCast(SimpleVectorStyle, points.m_style);
            vectorStyle->setColor(pointColor);
            m_elements[ElementType::POINTS] = points;

            OverlayElement selectedPoint;
            selectedPoint.m_style = StylePtr(Style::createStyle("simplePoint"));
            vectorStyle =
                    ngsDynamicCast(SimpleVectorStyle, selectedPoint.m_style);
            vectorStyle->setColor(selectedPointColor);
            m_elements[ElementType::SELECTED_POINT] = selectedPoint;
            break;
        }

        case wkbLineString:
        case wkbMultiLineString: {
            // TODO
            //m_style = StylePtr(Style::createStyle("simpleLine"));
            //SimpleVectorStyle* vectorStyle =
            //        ngsDynamicCast(SimpleVectorStyle, m_style);
            //vectorStyle->setColor(lineColor);
            break;
        }
        default:
            break; // Not supported yet
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

    auto addPoint = [](GlBuffer* buffer, const OGRPoint* pt, int index) {
        buffer->addVertex(pt->getX());
        buffer->addVertex(pt->getY());
        buffer->addVertex(0.0f);
        buffer->addIndex(index);
    };

    switch(type) {
        case wkbPoint: {
            freeGlBuffer(m_elements.at(ElementType::POINTS));
            freeGlBuffer(m_elements.at(ElementType::SELECTED_POINT));

            const OGRPoint* pt = static_cast<const OGRPoint*>(m_geometry.get());

            GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
            addPoint(buffer, pt, 0);

            VectorGlObject* bufferArray = new VectorGlObject();
            bufferArray->addBuffer(buffer);

            if(PointId(0) == m_selectedPointId) {
                m_elements.at(ElementType::SELECTED_POINT).m_glBuffer =
                        GlObjectPtr(bufferArray);
            } else {
                m_elements.at(ElementType::POINTS).m_glBuffer =
                        GlObjectPtr(bufferArray);
            }
            break;
        }
        case wkbMultiPoint: {
            freeGlBuffer(m_elements.at(ElementType::SELECTED_POINT));
            freeGlBuffer(m_elements.at(ElementType::POINTS));

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
                    addPoint(selBuffer, pt, 0);

                    VectorGlObject* selBufferArray = new VectorGlObject();
                    selBufferArray->addBuffer(selBuffer);

                    m_elements.at(ElementType::SELECTED_POINT).m_glBuffer =
                            GlObjectPtr(selBufferArray);
                    continue;
                }

                if(buffer->vertexSize() >= GlBuffer::maxVertices()) {
                    bufferArray->addBuffer(buffer);
                    index = 0;
                    buffer = new GlBuffer(GlBuffer::BF_PT);
                }
                addPoint(buffer, pt, index++);
            }

            bufferArray->addBuffer(buffer);
            m_elements.at(ElementType::POINTS).m_glBuffer =
                    GlObjectPtr(bufferArray);
            break;
        }
    }
}

void GlEditLayerOverlay::fillLine()
{
    // TODO
    //OGRLineString* line = static_cast<OGRLineString*>(m_geometry.get());

    //GlBuffer* buffer = new GlBuffer(GlBuffer::BF_LINE);
    // TODO

    //VectorGlObject* bufferArray = new VectorGlObject();
    //bufferArray->addBuffer(buffer);

    //return bufferArray;
}

void GlEditLayerOverlay::freeResources()
{
    EditLayerOverlay::freeResources();
    freeGlBuffers();
}

void GlEditLayerOverlay::freeGlBuffer(OverlayElement& element)
{
    if(element.m_glBuffer) {
        const GlView* constGlView = dynamic_cast<const GlView*>(&m_map);
        if(constGlView) {
            GlView* glView = const_cast<GlView*>(constGlView);
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
    m_elements.clear();
}

bool GlEditLayerOverlay::draw()
{
    if(!visible() || !m_elements.size()) {
        // !m_elements.size() should never happen.
        return true;
    }

    if(!m_elements.at(ElementType::SELECTED_POINT).m_glBuffer) {
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

            style->prepare(m_map.getSceneMatrix(), m_map.getInvViewMatrix(),
                    buff->type());
            style->draw(*buff);
        }
    }
    return true;
}

//------------------------------------------------------------------------------
// GlLocationOverlay
//------------------------------------------------------------------------------

GlLocationOverlay::GlLocationOverlay(const MapView& map) : LocationOverlay(map),
    GlRenderOverlay(),
    m_style(static_cast<PointStyle*>(Style::createStyle("primitivePoint")))
{
    m_style->setType(PT_DIAMOND);
    m_style->setColor({255,0,0,255});
}

bool GlLocationOverlay::setStyleName(const char* name)
{
    PointStyle* style = static_cast<PointStyle*>(Style::createStyle(name));
    if(nullptr == style) {
        return false;
    }
    const GlView* constGlView = dynamic_cast<const GlView*>(&m_map);
    if(constGlView) {
        GlView* glView = const_cast<GlView*>(constGlView);
        glView->freeResource(m_style);
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

    buffer.bind();
    m_style->prepare(m_map.getSceneMatrix(), m_map.getInvViewMatrix(),
                     buffer.type());
    m_style->draw(buffer);
    buffer.destroy();

    return true;
}

} // namespace ngs
