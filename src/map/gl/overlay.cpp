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

#include "map/gl/view.h"
#include "map/mapview.h"

// gdal
#include "ogr_core.h"

namespace ngs {

GlRenderOverlay::GlRenderOverlay()
{
}

GlEditLayerOverlay::GlEditLayerOverlay(const MapView& map)
        : EditLayerOverlay(map)
        , GlRenderOverlay()
{
}

void GlEditLayerOverlay::setVisible(bool visible)
{
    EditLayerOverlay::setVisible(visible);
    if(!visible) {
        freeResources();
    }
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
    freeResources();

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
            m_elements[ElementType::points] = points;

            OverlayElement selectedPoint;
            selectedPoint.m_style = StylePtr(Style::createStyle("simplePoint"));
            vectorStyle =
                    ngsDynamicCast(SimpleVectorStyle, selectedPoint.m_style);
            vectorStyle->setColor(selectedPointColor);
            m_elements[ElementType::selectedPoint] = selectedPoint;
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

    fill(false);
}

OGRGeometry* GlEditLayerOverlay::releaseGeometry()
{
    OGRGeometry* geom = EditLayerOverlay::releaseGeometry();
    freeResources();
    return geom;
}

void GlEditLayerOverlay::resetGeometry()
{
    EditLayerOverlay::resetGeometry();
    freeResources();
}

bool GlEditLayerOverlay::selectPoint(const OGRRawPoint& mapCoordinates)
{
    bool ret = EditLayerOverlay::selectPoint(mapCoordinates);
    if(ret) {
        fill(false);
    }
    return ret;
}

bool GlEditLayerOverlay::shiftPoint(const OGRRawPoint& mapOffset)
{
    bool ret = EditLayerOverlay::shiftPoint(mapOffset);
    if(ret) {
        fill(false);
    }
    return ret;
}

bool GlEditLayerOverlay::addGeometryPart(const OGRRawPoint& geometryCenter)
{
    bool ret = EditLayerOverlay::addGeometryPart(geometryCenter);
    if(ret) {
        fill(false);
    }
    return ret;
}

bool GlEditLayerOverlay::deleteGeometryPart()
{
    bool ret = EditLayerOverlay::deleteGeometryPart();
    if(ret) {
        fill(false);
    }
    return ret;
}

bool GlEditLayerOverlay::undo()
{
    bool ret = EditLayerOverlay::undo();
    if(ret) {
        fill(false);
    }
    return ret;
}

bool GlEditLayerOverlay::redo()
{
    bool ret = EditLayerOverlay::redo();
    if(ret) {
        fill(false);
    }
    return ret;
}

bool GlEditLayerOverlay::fill(bool /*isLastTry*/)
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
            freeResource(m_elements.at(ElementType::points));
            freeResource(m_elements.at(ElementType::selectedPoint));

            const OGRPoint* pt = static_cast<const OGRPoint*>(m_geometry.get());

            GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
            addPoint(buffer, pt, 0);

            VectorGlObject* bufferArray = new VectorGlObject();
            bufferArray->addBuffer(buffer);

            if(PointId(0) == m_selectedPointId) {
                m_elements.at(ElementType::selectedPoint).m_glBuffer =
                        GlObjectPtr(bufferArray);
            } else {
                m_elements.at(ElementType::points).m_glBuffer =
                        GlObjectPtr(bufferArray);
            }
            break;
        }
        case wkbMultiPoint: {
            freeResource(m_elements.at(ElementType::selectedPoint));
            freeResource(m_elements.at(ElementType::points));

            const OGRMultiPoint* mpt =
                    static_cast<const OGRMultiPoint*>(m_geometry.get());

            GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
            VectorGlObject* bufferArray = new VectorGlObject();

            int index = 0;
            for(int i = 0, num = mpt->getNumGeometries(); i < num; ++i) {
                const OGRPoint* pt =
                        static_cast<const OGRPoint*>(mpt->getGeometryRef(i));

                if(PointId(0, NOT_FOUND, i) == m_selectedPointId) {
                    GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
                    addPoint(buffer, pt, 0);

                    VectorGlObject* bufferArray = new VectorGlObject();
                    bufferArray->addBuffer(buffer);

                    m_elements.at(ElementType::selectedPoint).m_glBuffer =
                            GlObjectPtr(bufferArray);
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
            m_elements.at(ElementType::points).m_glBuffer =
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

void GlEditLayerOverlay::freeResource(OverlayElement& element)
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

void GlEditLayerOverlay::freeResources()
{
    for(auto it = m_elements.begin(); it != m_elements.end(); ++it) {
        freeResource(it->second);
    }
    m_elements.clear();
}

bool GlEditLayerOverlay::draw()
{
    if(!visible() || !m_elements.size()) {
        // !m_elements.size() should never happen.
        return true;
    }

    if(!m_elements.at(ElementType::selectedPoint).m_glBuffer) {
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

} // namespace ngs
