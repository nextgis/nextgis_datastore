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

void GlEditLayerOverlay::setGeometry(GeometryPtr geometry)
{
    EditLayerOverlay::setGeometry(geometry);

    switch(OGR_GT_Flatten(geometry->getGeometryType())) {
        case wkbPoint:
        case wkbMultiPoint: {
            m_style = StylePtr(Style::createStyle("simplePoint"));
            SimpleVectorStyle* vectorStyle =
                    ngsDynamicCast(SimpleVectorStyle, m_style);
            vectorStyle->setColor({255, 0, 0, 255});
            break;
        }

        case wkbLineString:
        case wkbMultiLineString: {
            m_style = StylePtr(Style::createStyle("simpleLine"));
            SimpleVectorStyle* vectorStyle =
                    ngsDynamicCast(SimpleVectorStyle, m_style);
            vectorStyle->setColor({255, 0, 0, 255});
            break;
        }
        default:
            break; // Not supported yet
    }

    fill(false);
}

bool GlEditLayerOverlay::shiftPoint(const OGRRawPoint& mapOffset)
{
    bool ret = EditLayerOverlay::shiftPoint(mapOffset);
    if(ret) {
        fill(false);
    }
    return ret;
}

bool GlEditLayerOverlay::fill(bool /*isLastTry*/)
{
    VectorGlObject* bufferArray = nullptr;

    switch(m_style->type()) {
        case Style::T_POINT:
            bufferArray = fillPoint();
            break;
        case Style::T_LINE:
            bufferArray = fillLine();
            break;
        case Style::T_FILL:
        case Style::T_IMAGE:
            break;
    }

    if(m_glBuffer) {
        const GlView* constGlView = dynamic_cast<const GlView*>(&m_map);
        if(constGlView) {
            GlView* glView = const_cast<GlView*>(constGlView);
            glView->freeResource(m_glBuffer);
        }
    }

    if(!bufferArray) {
        m_glBuffer = GlObjectPtr();
        return true;
    }

    m_glBuffer = GlObjectPtr(bufferArray);
    return true;
}

VectorGlObject* GlEditLayerOverlay::fillPoint()
{
    OGRwkbGeometryType type = OGR_GT_Flatten(m_geometry->getGeometryType());
    if(wkbPoint != type && wkbMultiPoint != type) {
        return nullptr;
    }

    auto addPoint = [](GlBuffer* buffer, const OGRPoint* pt, int index) {
        buffer->addVertex(pt->getX());
        buffer->addVertex(pt->getY());
        buffer->addVertex(0.0f);
        buffer->addIndex(index);
    };

    VectorGlObject* bufferArray = new VectorGlObject();
    GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);

    switch(type) {
        case wkbPoint: {
            const OGRPoint* pt = static_cast<const OGRPoint*>(m_geometry.get());
            addPoint(buffer, pt, 0);
            break;
        }
        case wkbMultiPoint: {
            const OGRMultiPoint* mpt =
                    static_cast<const OGRMultiPoint*>(m_geometry.get());
            int index = 0;
            for(int i = 0, num = mpt->getNumGeometries(); i < num; ++i) {
                if(buffer->vertexSize() >= GlBuffer::maxVertices()) {
                    bufferArray->addBuffer(buffer);
                    index = 0;
                    buffer = new GlBuffer(GlBuffer::BF_PT);
                }
                const OGRPoint* pt =
                        static_cast<const OGRPoint*>(mpt->getGeometryRef(i));
                addPoint(buffer, pt, index++);
            }
            break;
        }
    }

    bufferArray->addBuffer(buffer);
    return bufferArray;
}

VectorGlObject* GlEditLayerOverlay::fillLine()
{
    OGRLineString* line = static_cast<OGRLineString*>(m_geometry.get());

    GlBuffer* buffer = new GlBuffer(GlBuffer::BF_LINE);
    // TODO

    VectorGlObject* bufferArray = new VectorGlObject();
    bufferArray->addBuffer(buffer);

    return bufferArray;
}

bool GlEditLayerOverlay::draw()
{
    if(!visible() || !m_style) {
        // !m_style should never happened
        return true;
    }

    if(!m_glBuffer) {
        return false; // Data not yet loaded
    }

    VectorGlObject* vectorGlBuffer = ngsDynamicCast(VectorGlObject, m_glBuffer);
    for(const GlBufferPtr& buff : vectorGlBuffer->buffers()) {

        if(buff->bound()) {
            buff->rebind();
        } else {
            buff->bind();
        }

        m_style->prepare(
                m_map.getSceneMatrix(), m_map.getInvViewMatrix(), buff->type());
        m_style->draw(*buff);
    }
    return true;
}

} // namespace ngs
