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

GlEditLayerOverlay::GlEditLayerOverlay(MapView* map) : EditLayerOverlay(map),
    GlRenderOverlay()
{
    GlView* mapView = dynamic_cast<GlView*>(m_map);
    const TextureAtlas* atlas = mapView ? mapView->textureAtlas() : nullptr;

    PointStyle* pointStyle = static_cast<PointStyle*>(
                Style::createStyle("simpleEditPoint", atlas));
    if(pointStyle)
        m_pointStyle = PointStylePtr(pointStyle);

    EditLineStyle* lineStyle = static_cast<EditLineStyle*>(
                Style::createStyle("editLine", atlas));
    if(lineStyle)
        m_lineStyle = EditLineStylePtr(lineStyle);

    PointStyle* crossStyle = static_cast<PointStyle*>(
                Style::createStyle("simpleEditCross", atlas));
    if(crossStyle)
        m_crossStyle = PointStylePtr(crossStyle);
}

bool GlEditLayerOverlay::setStyleName(enum ngsEditStyleType type,
                                      const char* name)
{
    GlView* mapView = dynamic_cast<GlView*>(m_map);
    Style* style = Style::createStyle(name, mapView ? mapView->textureAtlas() :
                                                      nullptr);
    if(!style) {
        return false;
    }

    if(EST_POINT == type) {
        PointStyle* pointStyle = dynamic_cast<PointStyle*>(style);
        if(pointStyle) {
            freeGlStyle(m_pointStyle);
            m_pointStyle = PointStylePtr(pointStyle);
            return true;
        }
    }

    if(EST_LINE == type) {
        EditLineStyle* lineStyle = dynamic_cast<EditLineStyle*>(style);
        if(lineStyle) {
            freeGlStyle(m_lineStyle);
            m_lineStyle = EditLineStylePtr(lineStyle);
            return true;
        }
    }

    if(EST_CROSS == type) {
        PointStyle* crossStyle = dynamic_cast<PointStyle*>(style);
        if(crossStyle) {
            freeGlStyle(m_crossStyle);
            m_crossStyle = PointStylePtr(crossStyle);
            return true;
        }
    }

    delete style; // Delete unused style.
    return false;
}

bool GlEditLayerOverlay::setStyle(
        enum ngsEditStyleType type, const CPLJSONObject& jsonStyle)
{
    if(EST_POINT == type) {
        return m_pointStyle->load(jsonStyle);
    }
    if(EST_LINE == type) {
        return m_lineStyle->load(jsonStyle);
    }
    if(EST_CROSS == type) {
        return m_crossStyle->load(jsonStyle);
    }
    return false;
}

CPLJSONObject GlEditLayerOverlay::style(enum ngsEditStyleType type) const
{
    if(EST_POINT == type) {
        return m_pointStyle->save();
    }
    if(EST_LINE == type) {
        return m_lineStyle->save();
    }
    if(EST_CROSS == type) {
        return m_crossStyle->save();
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

bool GlEditLayerOverlay::addPoint()
{
    bool ret = EditLayerOverlay::addPoint();
    if(ret) {
        fill();
    }
    return ret;
}

bool GlEditLayerOverlay::deletePoint()
{
    bool ret = EditLayerOverlay::deletePoint();
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
    fill();
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

bool GlEditLayerOverlay::singleTap(const OGRRawPoint& mapCoordinates)
{
    bool ret = EditLayerOverlay::singleTap(mapCoordinates);
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

    freeGlBuffers();

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbPoint:
        case wkbMultiPoint: {
            fillPoints();
            break;
        }

        case wkbLineString:
        case wkbMultiLineString: {
            fillLines();
            break;
        }
        default:
            break; // Not supported yet
    }
    return true;
}

void GlEditLayerOverlay::fillPoints()
{
    if(!m_geometry)
        return;

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbPoint: {
            const OGRPoint* pt = static_cast<const OGRPoint*>(m_geometry.get());

            auto getPoint = [pt](int /*index*/) -> SimplePoint {
                SimplePoint spt;
                spt.x = static_cast<float>(pt->getX());
                spt.y = static_cast<float>(pt->getY());
                return spt;
            };

            auto isSelectedPoint = [this](int /*index*/) -> bool {
                return (m_selectedPointId.pointId() == 0);
            };

            fillPointElements(1, getPoint, isSelectedPoint);
            break;
        }
        case wkbMultiPoint: {
            const OGRMultiPoint* mpt =static_cast<const OGRMultiPoint*>(
                        m_geometry.get());

            auto getPoint = [mpt](int index) -> SimplePoint {
                const OGRPoint* pt = static_cast<const OGRPoint*>(
                        mpt->getGeometryRef(index));
                SimplePoint spt;
                spt.x = static_cast<float>(pt->getX());
                spt.y = static_cast<float>(pt->getY());
                return spt;
            };

            auto isSelectedPoint = [this](int index) -> bool {
                return (m_selectedPointId.geometryId() == index &&
                        m_selectedPointId.pointId() == 0);
            };

            fillPointElements(mpt->getNumGeometries(), getPoint, isSelectedPoint);
            break;
        }
    }
}

void GlEditLayerOverlay::fillLines()
{
    if(!m_geometry) {
        return;
    }

    switch(OGR_GT_Flatten(m_geometry->getGeometryType())) {
        case wkbLineString: {
            const OGRLineString* line = static_cast<const OGRLineString*>(
                        m_geometry.get());

            auto getLineFunc = [line](int /*index*/) -> const OGRLineString* {
                return line;
            };

            auto isSelectedLineFunc = [this](int /*index*/) -> bool {
                return (m_selectedPointId.pointId() >= 0);
            };

            fillLineElements(1, getLineFunc, isSelectedLineFunc);
            break;
        }

        case wkbMultiLineString: {
            const OGRMultiLineString* mline =
                    static_cast<const OGRMultiLineString*>(m_geometry.get());

            auto getLineFunc = [mline](int index) -> const OGRLineString* {
                return static_cast<const OGRLineString*>(
                        mline->getGeometryRef(index));
            };

            auto isSelectedLineFunc = [this](int index) -> bool {
                return (m_selectedPointId.geometryId() == index &&
                        m_selectedPointId.pointId() >= 0);
            };

            fillLineElements(mline->getNumGeometries(), getLineFunc,
                             isSelectedLineFunc);
            break;
        }
    }
}

void GlEditLayerOverlay::fillPointElements(int numPoints,
        GetPointFunc getPointFunc,
        IsSelectedGeometryFunc isSelectedPointFunc)
{
    EditPointStyle* editPointStyle = ngsDynamicCast(EditPointStyle, m_pointStyle);
    GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject* bufferArray = new VectorGlObject();
    GlBuffer* selBuffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject* selBufferArray = new VectorGlObject();

    int index = 0;
    for(int i = 0; i < numPoints; ++i) {
        SimplePoint pt = getPointFunc(i);

        if(isSelectedPointFunc(i)) {
            if(editPointStyle)
                editPointStyle->setEditElementType(EET_SELECTED_POINT);
            /*int selIndex = */
            m_pointStyle->addPoint(pt, 0.0f, 0, selBuffer);
            continue;
        }

        if(buffer->vertexSize() >= GlBuffer::maxVertices()) {
            bufferArray->addBuffer(buffer);
            index = 0;
            buffer = new GlBuffer(GlBuffer::BF_PT);
        }

        if(editPointStyle) {
            editPointStyle->setEditElementType(EET_POINT);
        }
        index = m_pointStyle->addPoint(pt, 0.0f,
                                       static_cast<unsigned short>(index),
                                       buffer);
    }

    bufferArray->addBuffer(buffer);
    m_elements[EET_POINT] = GlObjectPtr(bufferArray);
    selBufferArray->addBuffer(selBuffer);
    m_elements[EET_SELECTED_POINT] = GlObjectPtr(selBufferArray);
}

void GlEditLayerOverlay::fillMedianPointElements(int numPoints,
        GetPointFunc getPointFunc,
        IsSelectedGeometryFunc isSelectedMedianPointFunc)
{
    EditPointStyle* editPointStyle = ngsDynamicCast(EditPointStyle, m_pointStyle);
    GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject* bufferArray = new VectorGlObject();
    GlBuffer* selBuffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject* selBufferArray = new VectorGlObject();

    int index = 0;
    for(int i = 0; i < numPoints - 1; ++i) {
        SimplePoint pt1 = getPointFunc(i);
        SimplePoint pt2 = getPointFunc(i + 1);
        SimplePoint pt = ngsGetMedianPoint(pt1, pt2);

        if(isSelectedMedianPointFunc(i)) {
            if(editPointStyle)
                editPointStyle->setEditElementType(EET_SELECTED_MEDIAN_POINT);
            /*int selIndex = */
            m_pointStyle->addPoint(pt, 0.0f, 0, selBuffer);
            continue;
        }

        if(buffer->vertexSize() >= GlBuffer::maxVertices()) {
            bufferArray->addBuffer(buffer);
            index = 0;
            buffer = new GlBuffer(GlBuffer::BF_PT);
        }

        if(editPointStyle) {
            editPointStyle->setEditElementType(EET_MEDIAN_POINT);
        }
        index = m_pointStyle->addPoint(pt, 0.0f,
                                       static_cast<unsigned short>(index),
                                       buffer);
    }

    bufferArray->addBuffer(buffer);
    m_elements[EET_MEDIAN_POINT] = GlObjectPtr(bufferArray);
    selBufferArray->addBuffer(selBuffer);
    m_elements[EET_SELECTED_MEDIAN_POINT] = GlObjectPtr(selBufferArray);
}

void GlEditLayerOverlay::fillLineElements(int numLines,
        GetLineFunc getLineFunc,
        IsSelectedGeometryFunc isSelectedLineFunc)
{
    VectorGlObject* bufferArray = new VectorGlObject();
    VectorGlObject* selBufferArray = new VectorGlObject();

    for(int i = 0; i < numLines; ++i) {
        const OGRLineString* line = getLineFunc(i);
        int numPoints = line->getNumPoints();
        bool isSelectedLine = isSelectedLineFunc(i);

        m_lineStyle->setEditElementType(
                (isSelectedLine) ? EET_SELECTED_LINE : EET_LINE);

        if(isSelectedLine) {
            auto getPointFunc = [line](int index) -> SimplePoint {
                OGRPoint pt;
                line->getPoint(index, &pt);
                SimplePoint spt;
                spt.x = static_cast<float>(pt.getX());
                spt.y = static_cast<float>(pt.getY());
                return spt;
            };

            auto isSelectedPointFunc = [this](int index) -> bool {
                return (PointId(index).pointId() ==
                        m_selectedPointId.pointId());
            };

            auto isSelectedMedianPointFunc = [this](int /*index*/) -> bool {
                return false;
            };

            fillLineBuffers(line, selBufferArray);
            fillMedianPointElements(
                    numPoints, getPointFunc, isSelectedMedianPointFunc);
            fillPointElements(numPoints, getPointFunc, isSelectedPointFunc);
            continue;
        }

        fillLineBuffers(line, bufferArray);
    }

    m_elements[EET_LINE] = GlObjectPtr(bufferArray);
    m_elements[EET_SELECTED_LINE] = GlObjectPtr(selBufferArray);
}

void GlEditLayerOverlay::fillLineBuffers(const OGRLineString* line,
                                         VectorGlObject* bufferArray)
{
    auto getPointFunc = [line](int index) -> SimplePoint {
        OGRPoint pt;
        line->getPoint(index, &pt);
        SimplePoint spt;
        spt.x = static_cast<float>(pt.getX());
        spt.y = static_cast<float>(pt.getY());
        return spt;
    };

    int numPoints = line->getNumPoints();
    bool isClosedLine = line->get_IsClosed();

    GlBuffer* buffer = new GlBuffer(GlBuffer::BF_LINE);
    unsigned short index = 0;
    Normal prevNormal;

    auto createBufferIfNeed = [bufferArray, &buffer, &index](
                                      size_t amount) -> void {
        if(!buffer->canStoreVertices(amount, true)) {
            bufferArray->addBuffer(buffer);
            index = 0;
            buffer = new GlBuffer(GlBuffer::BF_LINE);
        }
    };

    for(int i = 0; i < numPoints - 1; ++i) {
        SimplePoint pt1 = getPointFunc(i);
        SimplePoint pt2 = getPointFunc(i + 1);
        Normal normal = ngsGetNormals(pt1, pt2);

        if(i == 0 || i == numPoints - 2) { // Add cap
            if(!isClosedLine) {
                if(i == 0) {
                    createBufferIfNeed(m_lineStyle->lineCapVerticesCount());
                    index = m_lineStyle->addLineCap(pt1, normal, 0.0f, index, buffer);
                }

                if(i == numPoints - 2) {
                    createBufferIfNeed(m_lineStyle->lineCapVerticesCount());

                    Normal reverseNormal;
                    reverseNormal.x = -normal.x;
                    reverseNormal.y = -normal.y;
                    index = m_lineStyle->addLineCap(
                            pt2, reverseNormal, 0.0f, index, buffer);
                }
            }
        }

        if(i != 0) { // Add join
            createBufferIfNeed(m_lineStyle->lineJoinVerticesCount());
            index = m_lineStyle->addLineJoin(
                    pt1, prevNormal, normal, 0.0f, index, buffer);
        }

        createBufferIfNeed(12);
        index = m_lineStyle->addSegment(pt1, pt2, normal, 0.0f, index, buffer);
        prevNormal = normal;
    }

    bufferArray->addBuffer(buffer);
}

void GlEditLayerOverlay::fillCrossElement()
{
    freeGlBuffer(m_elements[EET_CROSS]);

    GlBuffer* buffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject* bufferArray = new VectorGlObject();

    OGRRawPoint pt = m_map->getCenter();
    SimplePoint spt;
    spt.x = static_cast<float>(pt.x);
    spt.y = static_cast<float>(pt.y);

    m_crossStyle->addPoint(spt, 0.0f, 0, buffer);

    bufferArray->addBuffer(buffer);
    m_elements[EET_CROSS] = GlObjectPtr(bufferArray);
}

void GlEditLayerOverlay::freeResources()
{
    EditLayerOverlay::freeResources();
    freeGlBuffers();
    //freeGlStyles(); // TODO: only on close map
    //m_elements.clear();
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
    m_elements.clear();
}

bool GlEditLayerOverlay::draw()
{
    if(!visible() || !m_elements.size()) {
        // !m_elements.size() should never happen.
        return true;
    }

    if(crossVisible()) {
        fillCrossElement();
    } else {
        auto findIt = m_elements.find(EET_SELECTED_POINT);
        if(m_elements.end() == findIt || !findIt->second) {
            // One of the vertices must always be selected.
            return false; // Data is not yet loaded.
        }
    }

    for(auto it = m_elements.begin(); it != m_elements.end(); ++it) {
        GlObjectPtr glBuffer = it->second;
        if(!glBuffer) {
            continue;
        }

        enum ngsEditElementType styleType = it->first;
        Style* style = nullptr;
        if(EET_POINT == styleType ||
           EET_SELECTED_POINT == styleType ||
           EET_MEDIAN_POINT == styleType ||
           EET_SELECTED_MEDIAN_POINT == styleType) {
            style = m_pointStyle.get();

            EditPointStyle* editPointStyle = ngsDynamicCast(EditPointStyle,
                                                            m_pointStyle);
            if(editPointStyle) {
                editPointStyle->setEditElementType(it->first);
            }
        }
        if(EET_LINE == styleType || EET_SELECTED_LINE == styleType) {
            style = m_lineStyle.get();
            m_lineStyle->setEditElementType(it->first);
        }
        if(EET_CROSS == styleType) {
            style = m_crossStyle.get();
        }
        if(!style) {
            continue;
        }

        VectorGlObject* vectorGlBuffer = ngsDynamicCast(VectorGlObject, glBuffer);
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
    /*int index = */m_style->addPoint(m_location, 0.0f, 0, &buffer);
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
