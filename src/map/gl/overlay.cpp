/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016-2018 NextGIS, <info@nextgis.com>
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

#include "ds/earcut.hpp"
#include "map/gl/view.h"
#include "map/mapview.h"


namespace ngs {

constexpr double DELTA = 0.00000001;

//------------------------------------------------------------------------------
// GlRenderOverlay
//------------------------------------------------------------------------------

GlRenderOverlay::GlRenderOverlay()
{
}

//------------------------------------------------------------------------------
// GlEditLayerOverlay
//------------------------------------------------------------------------------

GlEditLayerOverlay::GlEditLayerOverlay(MapView *map) : EditLayerOverlay(map),
    GlRenderOverlay()
{
    GlView *mapView = dynamic_cast<GlView*>(m_map);
    if(mapView) {
        TextureAtlas atlas = mapView->textureAtlas();
        PointStyle *pointStyle = static_cast<PointStyle*>(
                    Style::createStyle("simpleEditPoint", atlas));
        if(pointStyle) {
            m_pointStyle = PointStylePtr(pointStyle);
        }

        EditLineStyle *lineStyle = static_cast<EditLineStyle*>(
                    Style::createStyle("editLine", atlas));
        if(lineStyle) {
            m_lineStyle = EditLineStylePtr(lineStyle);
        }

        EditFillStyle *fillStyle = static_cast<EditFillStyle*>(
                    Style::createStyle("editFill", atlas));
        if(fillStyle) {
            m_fillStyle = EditFillStylePtr(fillStyle);
        }

        PointStyle *crossStyle = static_cast<PointStyle*>(
                    Style::createStyle("simpleEditCross", atlas));
        if(crossStyle) {
            m_crossStyle = PointStylePtr(crossStyle);
        }
    }
}

bool GlEditLayerOverlay::setStyleName(enum ngsEditStyleType type,
                                      const std::string &name)
{
    GlView *mapView = dynamic_cast<GlView*>(m_map);
    if(nullptr == mapView) {
        return false;
    }

    Style *style = Style::createStyle(name, mapView->textureAtlas());
    if(!style) {
        return false;
    }

    if(EST_POINT == type) {
        PointStyle *pointStyle = dynamic_cast<PointStyle*>(style);
        if(pointStyle) {
            freeGlStyle(m_pointStyle);
            m_pointStyle = PointStylePtr(pointStyle);
            return true;
        }
    }

    if(EST_LINE == type) {
        EditLineStyle *lineStyle = dynamic_cast<EditLineStyle*>(style);
        if(lineStyle) {
            freeGlStyle(m_lineStyle);
            m_lineStyle = EditLineStylePtr(lineStyle);
            return true;
        }
    }

    if(EST_FILL == type) {
        EditFillStyle *fillStyle = dynamic_cast<EditFillStyle*>(style);
        if(fillStyle) {
            freeGlStyle(m_fillStyle);
            m_fillStyle = EditFillStylePtr(fillStyle);
            return true;
        }
    }

    if(EST_CROSS == type) {
        PointStyle *crossStyle = dynamic_cast<PointStyle*>(style);
        if(crossStyle) {
            freeGlStyle(m_crossStyle);
            m_crossStyle = PointStylePtr(crossStyle);
            return true;
        }
    }

    delete style; // Delete unused style.
    return false;
}

bool GlEditLayerOverlay::setStyle(enum ngsEditStyleType type,
                                  const CPLJSONObject& jsonStyle)
{
    if(EST_POINT == type) {
        return m_pointStyle->load(jsonStyle);
    }
    if(EST_LINE == type) {
        return m_lineStyle->load(jsonStyle);
    }
    if(EST_FILL == type) {
        return m_fillStyle->load(jsonStyle);
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
    if(EST_FILL == type) {
        return m_fillStyle->save();
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
    else {
        fill();
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

bool GlEditLayerOverlay::addPoint(double x, double y)
{
    bool ret = EditLayerOverlay::addPoint(x, y);
    if(ret) {
        fill();
    }
    return ret;
}

enum ngsEditDeleteResult GlEditLayerOverlay::deletePoint()
{
    enum ngsEditDeleteResult ret = EditLayerOverlay::deletePoint();
    if(ret != EDT_FAILED) {
        fill();
    }
    return ret;
}

bool GlEditLayerOverlay::addHole()
{
    bool ret = EditLayerOverlay::addHole();
    if(ret) {
        fill();
    }
    return ret;
}

enum ngsEditDeleteResult GlEditLayerOverlay::deleteHole()
{
    enum ngsEditDeleteResult ret = EditLayerOverlay::deleteHole();
    if(ret != EDT_FAILED) {
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

enum ngsEditDeleteResult GlEditLayerOverlay::deleteGeometryPart()
{
    enum ngsEditDeleteResult ret = EditLayerOverlay::deleteGeometryPart();
    if(ret != EDT_FAILED) {
        fill();
    }
    return ret;
}

ngsPointId GlEditLayerOverlay::touch(double x, double y, enum ngsMapTouchType type)
{
    ngsPointId out = EditLayerOverlay::touch(x, y, type);
    if(type == ngsMapTouchType::MTT_SINGLE) {
        fill();
    }
    return out;
}

bool GlEditLayerOverlay::fill()
{
    if(!m_editGeometry) {
        return false;
    }

    freeGlBuffers();

    switch(m_editGeometry->type()) {
    case EditGeometry::Type::POINT: {
        EditPoint *eg = ngsDynamicCast(EditPoint, m_editGeometry);
        if(eg) {
            fillPointElements(std::vector<OGRRawPoint>({eg->data()}),
                              eg->selectedPoint());
            return true;
        }
        break;
    }

    case EditGeometry::Type::MULTIPOINT: {
        EditMultiPoint *eg = ngsDynamicCast(EditMultiPoint, m_editGeometry);
        if(eg) {
            fillPointElements(eg->data(), eg->selectedPoint());
            return true;
        }
        break;
    }

    case EditGeometry::Type::LINE: {
        EditLine *eg = ngsDynamicCast(EditLine, m_editGeometry);
        if(eg) {
            fillLineElements(std::vector<Line>({eg->data()}),
                             eg->selectedPart(), eg->selectedPoint());
            return true;
        }
        break;
    }

    case EditGeometry::Type::MULTILINE: {
        EditMultiLine *eg = ngsDynamicCast(EditMultiLine, m_editGeometry);
        if(eg) {
            fillLineElements(eg->data(), eg->selectedPart(), eg->selectedPoint());
            return true;
        }
        break;
    }

    case EditGeometry::Type::POLYGON: {
        EditPolygon *eg = ngsDynamicCast(EditPolygon, m_editGeometry);
        if(eg) {
            fillPolygonElements(std::vector<Polygon>({eg->data()}),
                                eg->selectedPart(), eg->selectedRing(),
                                eg->selectedPoint());
            return true;
        }
        break;
    }


    case EditGeometry::Type::MULTIPOLYGON: {
        EditMultiPolygon *eg = ngsDynamicCast(EditMultiPolygon, m_editGeometry);
        if(eg) {
            fillPolygonElements(eg->data(), eg->selectedPart(), eg->selectedRing(),
                             eg->selectedPoint());
            return true;
        }
        break;
    }
    }
    return false;
}

void GlEditLayerOverlay::fillPointElements(const std::vector<OGRRawPoint> &points,
                                           int selectedPointId)
{
    EditPointStyle *editPointStyle = ngsDynamicCast(EditPointStyle, m_pointStyle);
    GlBuffer *buffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject *bufferArray = new VectorGlObject();
    GlBuffer *selBuffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject *selBufferArray = new VectorGlObject();

    enum ngsEditElementType elementType = (m_walkingMode) ? EET_WALK_POINT :
                                                            EET_POINT;

    unsigned short index = 0;
    int pointIndex = -1;
    for(const OGRRawPoint &point : points) {
        SimplePoint pt = {static_cast<float>(point.x),
                          static_cast<float>(point.y)};
        pointIndex++;
        if(pointIndex == selectedPointId) {
            if(editPointStyle) {
                editPointStyle->setEditElementType(EET_SELECTED_POINT);
            }
            m_pointStyle->addPoint(pt, 0.0f, 0, selBuffer);
            continue;
        }

        if(buffer->vertexSize() >= GlBuffer::maxVertices()) {
            bufferArray->addBuffer(buffer);
            index = 0;
            buffer = new GlBuffer(GlBuffer::BF_PT);
        }

        if(editPointStyle) {
            editPointStyle->setEditElementType(elementType);
        }
        index = m_pointStyle->addPoint(pt, 0.0f, index, buffer);
    }

    bufferArray->addBuffer(buffer);
    m_elements[elementType] = GlObjectPtr(bufferArray);
    selBufferArray->addBuffer(selBuffer);
    m_elements[EET_SELECTED_POINT] = GlObjectPtr(selBufferArray);
}

void GlEditLayerOverlay::fillMiddlePointElements(
        const std::vector<OGRRawPoint> &points)
{
    EditPointStyle *editPointStyle = ngsDynamicCast(EditPointStyle, m_pointStyle);
    GlBuffer *buffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject *bufferArray = new VectorGlObject();
    GlBuffer *selBuffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject *selBufferArray = new VectorGlObject();

    int index = 0;
    size_t numPoints = points.size();
    for(size_t i = 0; i < numPoints - 1; ++i) {
        OGRRawPoint medianPoint = ngsGetMiddlePoint(points[i], points[i + 1]);
        SimplePoint pt = {static_cast<float>(medianPoint.x),
                          static_cast<float>(medianPoint.y)};

//        if(isSelectedMedianPointFunc(i)) {
//            if(editPointStyle) {
//                editPointStyle->setEditElementType(EET_SELECTED_MEDIAN_POINT);
//            }
//            m_pointStyle->addPoint(pt, 0.0f, 0, selBuffer);
//            continue;
//        }

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

void GlEditLayerOverlay::fillLineElements(std::vector<Line> lines,
                                          int selectedLine, int selectedPoint,
                                          bool addToBuffer)
{
    VectorGlObject *bufferArray = nullptr;
    VectorGlObject *selBufferArray = nullptr;
    if(addToBuffer) {
        bufferArray = static_cast<VectorGlObject*>(m_elements[EET_LINE].get());
        selBufferArray = static_cast<VectorGlObject*>(
                m_elements[EET_SELECTED_LINE].get());
    }

    bool insertToLineElements = false;
    if(!bufferArray) {
        bufferArray = new VectorGlObject();
        insertToLineElements = true;
    }
    bool insertToSelLineElements = false;
    if(!selBufferArray) {
        selBufferArray = new VectorGlObject();
        insertToSelLineElements = true;
    }

    int currentLine = -1;
    for(const Line &line : lines) {
        currentLine++;
        bool isSelected = currentLine == selectedLine;

        m_lineStyle->setEditElementType((isSelected) ? EET_SELECTED_LINE :
                                                       EET_LINE);

        if(isSelected) {

            fillLineBuffers(line, selBufferArray);

            if(!m_walkingMode) {
                fillMiddlePointElements(line);
            }

            fillPointElements(line, selectedPoint);
            continue;
        }

        fillLineBuffers(line, bufferArray);
    }

    if(insertToLineElements) {
        m_elements[EET_LINE] = GlObjectPtr(bufferArray);
    }
    if(insertToSelLineElements) {
        m_elements[EET_SELECTED_LINE] = GlObjectPtr(selBufferArray);
    }
}

void GlEditLayerOverlay::fillLineBuffers(const Line &line,
                                         VectorGlObject* bufferArray)
{
    GlBuffer *buffer = new GlBuffer(GlBuffer::BF_LINE);
    size_t numPoints = line.size();

    if(numPoints > 0) {
        bool isClosedLine = ngsIsNear(line.front(), line.back(), DELTA);
        unsigned short index = 0;
        Normal prevNormal;

        auto createBufferIfNeed = [bufferArray, &buffer, &index](size_t amount) -> void {
            if(!buffer->canStoreVertices(amount, true)) {
                bufferArray->addBuffer(buffer);
                index = 0;
                buffer = new GlBuffer(GlBuffer::BF_LINE);
            }
        };

        for(size_t i = 0; i < numPoints - 1; ++i) {
            SimplePoint pt1 = { static_cast<float>(line[i].x),
                                static_cast<float>(line[i].y) };
            SimplePoint pt2 = { static_cast<float>(line[i + 1].x),
                                static_cast<float>(line[i + 1].y) };
            Normal normal = ngsGetNormals(pt1, pt2);

            if(i == 0 || i == numPoints - 2) { // Add cap
                if(!isClosedLine) {
                    if(i == 0) {
                        createBufferIfNeed(m_lineStyle->lineCapVerticesCount());
                        index = m_lineStyle->addLineCap(
                                pt1, normal, 0.0f, index, buffer);
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
            index = m_lineStyle->addSegment(
                    pt1, pt2, normal, 0.0f, index, buffer);
            prevNormal = normal;
        }
    }

    bufferArray->addBuffer(buffer);
}

void GlEditLayerOverlay::fillPolygonElements(std::vector<Polygon> polygons,
                                             int selectedPart, int selectedRing,
                                             int selectedPoint)
{
    VectorGlObject *bufferArray = new VectorGlObject();
    VectorGlObject *selBufferArray = new VectorGlObject();

    for(size_t i = 0; i < polygons.size(); ++i) {
        const Polygon &polygon = polygons[i];
        bool isSelected = selectedPart == static_cast<int>(i);

        m_fillStyle->setEditElementType((isSelected) ?
                                            EET_SELECTED_POLYGON : EET_POLYGON);

        if(isSelected) {
            fillPolygonBuffers(polygon, selBufferArray);
            fillLineElements(polygon, selectedRing, selectedPoint, true);
            continue;
        }

        fillPolygonBuffers(polygon, bufferArray);
        fillLineElements(polygon, selectedRing, selectedPoint, true);
    }

    m_elements[EET_POLYGON] = GlObjectPtr(bufferArray);
    m_elements[EET_SELECTED_POLYGON] = GlObjectPtr(selBufferArray);
}

void GlEditLayerOverlay::fillPolygonBuffers(const Polygon &polygon,
                                            VectorGlObject *bufferArray)
{
    // The number type to use for tessellation.
    using Coord = double;

    // The index type. Defaults to uint32_t, but you can also pass uint16_t
    // if you know that your data won't have more than 65536 vertices.
    using N = unsigned short;

    using MBPoint = std::array<Coord, 2>;

    // Create array.
    std::vector<std::vector<MBPoint>> mbPolygon;

    for(const Line &ring : polygon) {
        std::vector<MBPoint> mbRing;
        for(const OGRRawPoint &point : ring) {
            mbRing.emplace_back(MBPoint({{point.x, point.y}}));
        }
        mbPolygon.emplace_back(mbRing);
    }

    // Run tessellation.
    // Returns array of indices that refer to the vertices of the input polygon.
    // Three subsequent indices form a triangle.
    std::vector<N> mbIndices = mapbox::earcut<N>(mbPolygon);

	// Fill triangles.
    GlBuffer *fillBuffer = new GlBuffer(GlBuffer::BF_FILL);
    unsigned short index = 0;
    for(N mbIndex : mbIndices) {
        if(!fillBuffer->canStoreVertices(mbIndices.size() * 3)) {
            bufferArray->addBuffer(fillBuffer);
            index = 0;
            fillBuffer = new GlBuffer(GlBuffer::BF_FILL);
        }

        N currentIndex = mbIndex;
        for(const auto& ring : mbPolygon) {
            if(currentIndex >= ring.size()) {
                currentIndex -= ring.size();
                continue;
            }

            MBPoint mbPt = ring[currentIndex];
            fillBuffer->addVertex(static_cast<float>(mbPt[0]));
            fillBuffer->addVertex(static_cast<float>(mbPt[1]));
            fillBuffer->addVertex(0.0f);
            fillBuffer->addIndex(index++);
            break;
        }
    }
    bufferArray->addBuffer(fillBuffer);
}

void GlEditLayerOverlay::fillCrossElement()
{
    freeGlBuffer(m_elements[EET_CROSS]);

    GlBuffer *buffer = new GlBuffer(GlBuffer::BF_PT);
    VectorGlObject *bufferArray = new VectorGlObject();

    OGRRawPoint pt = m_map->getCenter();
    SimplePoint spt = { static_cast<float>(pt.x),
                        static_cast<float>(pt.y) };

    m_crossStyle->addPoint(spt, 0.0f, 0, buffer);

    bufferArray->addBuffer(buffer);
    m_elements[EET_CROSS] = GlObjectPtr(bufferArray);
}

void GlEditLayerOverlay::freeGlStyle(StylePtr style)
{
    if(style) {
        GlView *glView = dynamic_cast<GlView*>(m_map);
        if(glView) {
            glView->freeResource(style);
        }
    }
}

void GlEditLayerOverlay::freeGlBuffer(GlObjectPtr buffer)
{
    if(buffer) {
        GlView *glView = dynamic_cast<GlView*>(m_map);
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
    if(!visible()) {
        return true;
    }

    if(isCrossVisible()) {
        fillCrossElement();
    }
    else if(!m_walkingMode) {
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
        Style *style = nullptr;
        if(EET_POINT == styleType ||
           EET_SELECTED_POINT == styleType ||
           EET_WALK_POINT == styleType ||
           EET_MEDIAN_POINT == styleType ||
           EET_SELECTED_MEDIAN_POINT == styleType) {
            style = m_pointStyle.get();

            EditPointStyle *editPointStyle = ngsDynamicCast(EditPointStyle,
                                                            m_pointStyle);
            if(editPointStyle) {
                editPointStyle->setEditElementType(it->first);
            }
        }
        if(EET_LINE == styleType || EET_SELECTED_LINE == styleType) {
            style = m_lineStyle.get();
            m_lineStyle->setEditElementType(it->first);
        }
        if(EET_POLYGON == styleType || EET_SELECTED_POLYGON == styleType) {
            style = m_fillStyle.get();
            m_fillStyle->setEditElementType(it->first);
        }
        if(EET_CROSS == styleType) {
            style = m_crossStyle.get();
        }
        if(!style) {
            continue;
        }

        VectorGlObject *vectorGlBuffer = ngsDynamicCast(VectorGlObject, glBuffer);
        for(const GlBufferPtr& buff : vectorGlBuffer->buffers()) {
            if(buff->bound()) {
                buff->rebind();
            }
            else {
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

GlLocationOverlay::GlLocationOverlay(MapView *map) : LocationOverlay(map),
    GlRenderOverlay(),
    m_style(static_cast<PointStyle*>(Style::createStyle("simpleLocation",
                                                        TextureAtlas())))
{
    m_style->setType(PT_DIAMOND);
    m_style->setColor({255,0,0,255});
}

bool GlLocationOverlay::setStyleName(const std::string &name)
{
    if(compare(name, m_style->name())) {
        return true;
    }

    GlView *mapView = dynamic_cast<GlView*>(m_map);
    if(mapView) {
        PointStyle *style = static_cast<PointStyle*>(
                    Style::createStyle(name, mapView->textureAtlas()));
        if(nullptr == style) {
            return false;
        }

        mapView->freeResource(m_style);
        m_style = PointStylePtr(style);
    }
    return true;
}

bool GlLocationOverlay::setStyle(const CPLJSONObject &style)
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
    m_style->addPoint(m_location, 0.0f, 0, &buffer);
    LocationStyle *lStyle = ngsDynamicCast(LocationStyle, m_style);
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
