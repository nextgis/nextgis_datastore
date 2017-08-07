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

#include "map/mapview.h"

// gdal
#include "ogr_core.h"

namespace ngs
{

GlRenderOverlay::GlRenderOverlay()
        : m_dataMutex(CPLCreateMutex())
{
    CPLReleaseMutex(m_dataMutex);
}

GlRenderOverlay::~GlRenderOverlay()
{
    CPLDestroyMutex(m_dataMutex);
}

GlEditLayerOverlay::GlEditLayerOverlay(const MapView& map)
        : EditLayerOverlay(map)
        , GlRenderOverlay()
{
}

void GlEditLayerOverlay::setGeometry(GeometryPtr geometry)
{
    EditLayerOverlay::setGeometry(geometry);

    switch (OGR_GT_Flatten(geometry->getGeometryType())) {
        case wkbPoint:
        case wkbMultiPoint: {
            m_style = StylePtr(Style::createStyle("simplePoint"));
            SimpleVectorStyle* vectorStyle =
                    ngsDynamicCast(SimpleVectorStyle, m_style);
            ngsRGBA color{255, 0, 0, 255};
            vectorStyle->setColor(color);
            break;
        }
        default:
            break;  // Not supported yet
    }

    fill(false);
}

bool GlEditLayerOverlay::fill(bool /*isLastTry*/)
{
    double lockTime = CPLAtofM(CPLGetConfigOption("HTTP_TIMEOUT", "5"));
    VectorGlObject* bufferArray = nullptr;

    switch (m_style->type()) {
        case Style::T_POINT:
            bufferArray = fillPoint();
            break;
        case Style::T_LINE:
        case Style::T_FILL:
        case Style::T_IMAGE:
            break;
    }

    if (!bufferArray) {
        CPLMutexHolder holder(m_dataMutex, lockTime);
        m_glBuffer = GlObjectPtr();
        return true;
    }

    CPLMutexHolder holder(m_dataMutex, lockTime);
    m_glBuffer = GlObjectPtr(bufferArray);

    return true;
}

VectorGlObject* GlEditLayerOverlay::fillPoint()
{
    OGRPoint* pt = static_cast<OGRPoint*>(m_geometry.get());

    GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
    buffer->addVertex(pt->getX());
    buffer->addVertex(pt->getY());
    buffer->addVertex(0.0f);
    buffer->addIndex(0);

    VectorGlObject* bufferArray = new VectorGlObject();
    bufferArray->addBuffer(buffer);

    return bufferArray;
}

bool GlEditLayerOverlay::draw()
{
    if (!visible() || !m_style) {
        // !m_style should never happened
        return true;
    }

    CPLMutexHolder holder(m_dataMutex, 0.5);

    if (!m_glBuffer) {
        return false;  // Data not yet loaded
    }

    VectorGlObject* vectorGlBuffer = ngsDynamicCast(VectorGlObject, m_glBuffer);
    for (const GlBufferPtr& buff : vectorGlBuffer->buffers()) {

        if (buff->bound()) {
            buff->rebind();
        } else {
            buff->bind();
        }

        m_style->prepare(m_map.getSceneMatrix(), m_map.getInvViewMatrix(),
                         buff->type());
        m_style->draw(*buff);
    }
    return true;
}

}  // namespace ngs
