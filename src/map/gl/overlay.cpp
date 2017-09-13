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

GlEditLayerOverlay::GlEditLayerOverlay(MapView* map)
        : EditLayerOverlay(map)
        , GlRenderOverlay()
{
    GlView* mapView = dynamic_cast<GlView*>(m_map);
    const TextureAtlas* atlas = mapView ? mapView->textureAtlas() : nullptr;

    PointStyle* pointStyle = static_cast<PointStyle*>(
            Style::createStyle("simpleEditPointStyle", atlas));
    if(pointStyle)
        m_pointStyle = PointStylePtr(pointStyle);

    EditLineStyle* lineStyle = static_cast<EditLineStyle*>(
            Style::createStyle("editLineStyle", atlas));
    if(lineStyle)
        m_lineStyle = EditLineStylePtr(lineStyle);
}

bool GlEditLayerOverlay::setStyleName(
        enum ngsEditStyleType type, const char* name)
{
    GlView* mapView = dynamic_cast<GlView*>(m_map);
    Style* style = Style::createStyle(
            name, mapView ? mapView->textureAtlas() : nullptr);
    if(!style)
        return false;

    if(EST_POINT == type) {
        PointStyle* pointStyle = dynamic_cast<PointStyle*>(style);
        if(pointStyle) {
            freeGlStyle(m_pointStyle);
            m_pointStyle = PointStylePtr(pointStyle);
            return true;
        }
    }

    return false;
}

bool GlEditLayerOverlay::setStyle(
        enum ngsEditStyleType type, const CPLJSONObject& jsonStyle)
{
    if(EST_POINT == type) {
        return m_pointStyle->load(jsonStyle);
    }
    return false;
}

CPLJSONObject GlEditLayerOverlay::style(enum ngsEditStyleType type) const
{
    if(EST_POINT == type) {
        return m_pointStyle->save();
    }
    return CPLJSONObject();
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
    if(!m_geometry)
        return;

    EditPointStyle* editPointStyle =
            ngsDynamicCast(EditPointStyle, m_pointStyle);

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbPoint: {
            auto it = m_elements.find(EET_POINT);
            if(m_elements.end() != it)
                freeGlBuffer(it->second);
            it = m_elements.find(EET_SELECTED_POINT);
            if(m_elements.end() != it)
                freeGlBuffer(it->second);

            enum ngsEditElementType styleType =
                    (PointId(0) == m_selectedPointId) ? EET_SELECTED_POINT
                                                      : EET_POINT;
            const OGRPoint* pt = static_cast<const OGRPoint*>(m_geometry.get());

            GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
            SimplePoint spt;
            spt.x = static_cast<float>(pt->getX());
            spt.y = static_cast<float>(pt->getY());
            if(editPointStyle)
                editPointStyle->setType(styleType);
            /*int index = */
            m_pointStyle->addPoint(spt, 0, buffer);

            VectorGlObject* bufferArray = new VectorGlObject();
            bufferArray->addBuffer(buffer);
            m_elements[styleType] = GlObjectPtr(bufferArray);
            break;
        }
        case wkbMultiPoint: {
            auto it = m_elements.find(EET_POINT);
            if(m_elements.end() != it)
                freeGlBuffer(it->second);
            it = m_elements.find(EET_SELECTED_POINT);
            if(m_elements.end() != it)
                freeGlBuffer(it->second);

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
                    SimplePoint spt;
                    spt.x = static_cast<float>(pt->getX());
                    spt.y = static_cast<float>(pt->getY());
                    if(editPointStyle)
                        editPointStyle->setType(EET_SELECTED_POINT);
                    /*int selIndex = */
                    m_pointStyle->addPoint(spt, 0, selBuffer);

                    VectorGlObject* selBufferArray = new VectorGlObject();
                    selBufferArray->addBuffer(selBuffer);

                    m_elements[EET_SELECTED_POINT] =
                            GlObjectPtr(selBufferArray);
                    continue;
                }

                if(buffer->vertexSize() >= GlBuffer::maxVertices()) {
                    bufferArray->addBuffer(buffer);
                    index = 0;
                    buffer = new GlBuffer(GlBuffer::BF_PT);
                }

                SimplePoint spt;
                spt.x = static_cast<float>(pt->getX());
                spt.y = static_cast<float>(pt->getY());
                if(editPointStyle)
                    editPointStyle->setType(EET_POINT);
                index = m_pointStyle->addPoint(spt, index, buffer);
            }

            bufferArray->addBuffer(buffer);
            m_elements[EET_POINT] = GlObjectPtr(bufferArray);
            break;
        }
    }
}

void GlEditLayerOverlay::fillLine()
{
    if(!m_geometry)
        return;

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbLineString: {
            auto it = m_elements.find(EET_LINE);
            if(m_elements.end() != it)
                freeGlBuffer(it->second);
            it = m_elements.find(EET_SELECTED_LINE);
            if(m_elements.end() != it)
                freeGlBuffer(it->second);

//            enum ngsEditElementType styleType = // TODO
//                    (0 <= m_selectedPointId.pointId()) ? EET_SELECTED_LINE
//                                                       : EET_LINE;
            const OGRLineString* line =
                    static_cast<const OGRLineString*>(m_geometry.get());

            GlBuffer* buffer = new GlBuffer(GlBuffer::BF_LINE);
            VectorGlObject* bufferArray = new VectorGlObject();
            unsigned short index = 0;

            // Check if line is closed or not
            bool closed = line->get_IsClosed();

            Normal prevNormal;
            for(int i = 0; i < line->getNumPoints() - 1; ++i) {
                OGRPoint p1;
                line->getPoint(i, &p1);
                SimplePoint pt1;
                pt1.x = static_cast<float>(p1.getX());
                pt1.y = static_cast<float>(p1.getY());

                OGRPoint p2;
                line->getPoint(i + 1, &p2);
                SimplePoint pt2;
                pt2.x = static_cast<float>(p2.getX());
                pt2.y = static_cast<float>(p2.getY());

                Normal normal = ngsGetNormals(pt1, pt2);

                if(i == 0 || i == line->getNumPoints() - 2) { // Add cap
                    if(!closed) {
                        if(i == 0) {
                            if(!buffer->canStoreVertices(
                                       m_lineStyle->lineCapVerticesCount(),
                                       true)) {
                                bufferArray->addBuffer(buffer);
                                index = 0;
                                buffer = new GlBuffer(GlBuffer::BF_LINE);
                            }
                            index = m_lineStyle->addLineCap(
                                    pt1, normal, index, buffer);
                        }

                        if(i == line->getNumPoints() - 2) {
                            if(!buffer->canStoreVertices(
                                       m_lineStyle->lineCapVerticesCount(),
                                       true)) {
                                bufferArray->addBuffer(buffer);
                                index = 0;
                                buffer = new GlBuffer(GlBuffer::BF_LINE);
                            }

                            Normal reverseNormal;
                            reverseNormal.x = -normal.x;
                            reverseNormal.y = -normal.y;
                            index = m_lineStyle->addLineCap(
                                    pt2, reverseNormal, index, buffer);
                        }
                    }
                }

                if(i != 0) { // Add join
                    if(!buffer->canStoreVertices(
                               m_lineStyle->lineJoinVerticesCount(), true)) {
                        bufferArray->addBuffer(buffer);
                        index = 0;
                        buffer = new GlBuffer(GlBuffer::BF_LINE);
                    }
                    index = m_lineStyle->addLineJoin(
                            pt1, prevNormal, normal, index, buffer);
                }

                if(!buffer->canStoreVertices(12, true)) {
                    bufferArray->addBuffer(buffer);
                    index = 0;
                    buffer = new GlBuffer(GlBuffer::BF_LINE);
                }

                index = m_lineStyle->addSegment(
                        pt1, pt2, normal, index, buffer);
                prevNormal = normal;
            }

            bufferArray->addBuffer(buffer);
            m_elements[EET_LINE] = GlObjectPtr(bufferArray);
            break;
        }

        case wkbMultiLineString: {
            break;
        }
    }
}

void GlEditLayerOverlay::freeResources()
{
    EditLayerOverlay::freeResources();
    freeGlBuffers();
    //freeGlStyles(); // TODO: only on close map
    //m_elements.clear(); // TODO: only on close map
}

void GlEditLayerOverlay::freeGlStyle(StylePtr style)
{
    if(style) {
        GlView* glView = dynamic_cast<GlView*>(m_map);
        if(glView) {
            glView->freeResource(style);
        }
    }
}

void GlEditLayerOverlay::freeGlBuffer(GlObjectPtr buffer)
{
    if(buffer) {
        GlView* glView = dynamic_cast<GlView*>(m_map);
        if(glView) {
            glView->freeResource(buffer);
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

    auto findIt = m_elements.find(EET_SELECTED_POINT);
    if(m_elements.end() == findIt || !findIt->second) {
        // One of the vertices must always be selected.
//        return false; // Data is not yet loaded. // TODO
    }

    for(auto it = m_elements.begin(); it != m_elements.end(); ++it) {
        GlObjectPtr glBuffer = it->second;
        if(!glBuffer)
            continue;

        enum ngsEditElementType styleType = it->first;
        Style* style = nullptr;
        if(EET_POINT == styleType || EET_SELECTED_POINT == styleType
                || EET_MEDIAN_POINT == styleType
                || EET_SELECTED_MEDIAN_POINT == styleType) {
            style = m_pointStyle.get();

            EditPointStyle* editPointStyle =
                    ngsDynamicCast(EditPointStyle, m_pointStyle);
            if(editPointStyle)
                editPointStyle->setType(it->first);
        }
        if(EET_LINE == styleType || EET_SELECTED_LINE == styleType) {
            style = m_lineStyle.get();
//            m_lineStyle->setType(it->first); // TODO
        }
        if(!style)
            continue;

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
