/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*  Author: NikitaFeodonit, nfeodonit@yandex.com
*******************************************************************************
*  Copyright (C) 2016-2017 NextGIS, <info@nextgis.com>
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "map/glfillers.h"

#include "cpl_error.h"

#include "ngstore/util/constants.h"


using namespace ngs;

LineStringFiller::LineStringFiller(const OGRLineString* line,
        float level,
        int_least8_t crossExtent,
        const LineCapType layoutLineCap,
        const LineJoinType layoutLineJoin,
        const GlBufferSharedPtr currBuffer)
        : m_line(line)
        , m_level(level)
        , m_crossExtent(crossExtent)
        , m_layoutLineCap(layoutLineCap)
        , m_layoutLineJoin(layoutLineJoin)
        , m_currBuffer(currBuffer)
        , m_firstPt()
        , m_lastPt()
        , m_currPt()
        , m_prevPt()
        , m_nextPt()
        , m_prevNormal()
        , m_nextNormal()
        , m_startOfLine(true)
        , m_e1(-1)
        , m_e2(-1)
        , m_e3(-1)
{
    m_numPoints = m_line->getNumPoints();

    m_miterLimit = (LineJoinType::Bevel == m_layoutLineJoin)
            ? 1.05f
            : 2.50f /* float(layout.get<LineMiterLimit>()) */;  // TODO:

    //m_sharpCornerOffset = SHARP_CORNER_OFFSET  // TODO:
    //        * (float(EXTENT) / (tileSize /* tileSize * overscaling */));

    m_line->getPoint(0, &m_firstPt);
    m_line->getPoint(m_numPoints - 1, &m_lastPt);

    // For closed line string last point == first point, see
    // https://en.wikipedia.org/wiki/Well-known_text
    m_closed = (m_firstPt == m_lastPt);

    // TODO:
    //if (2 == m_numPoints && m_closed)
    //    return;

    m_beginCap = m_layoutLineCap;
    m_endCap = m_closed ? LineCapType::Butt : m_layoutLineCap;

    if (m_closed) {
        m_line->getPoint(m_numPoints - 2, &m_currPt);
        m_nextNormal = m_firstPt.normal(m_currPt);
    }
}

LineStringFiller::~LineStringFiller()
{
}

/*
 * Sharp corners cause dashed lines to tilt because the distance along the line
 * is the same at both the inner and outer corners. To improve the appearance of
 * dashed lines we add extra points near sharp corners so that a smaller part
 * of the line is tilted.
 *
 * COS_HALF_SHARP_CORNER controls how sharp a corner has to be for us to add an
 * extra vertex. The default is 75 degrees.
 *
 * The newly created vertices are placed SHARP_CORNER_OFFSET pixels
 * from the corner.
 */  // TODO:
//const float COS_HALF_SHARP_CORNER = std::cos(75.0 / 2.0 * (M_PI / 180.0));
//const float SHARP_CORNER_OFFSET = 15.0f;

/*
 * The maximum extent of a feature that can be safely stored in the buffer.
 * In practice, all features are converted to this extent before being added.
 *
 * Positions are stored as signed 16bit integers.
 * One bit is lost
 * for signedness to support features extending past the left edge of the tile.
 * One bit is lost
 * because the line vertex buffer packs 1 bit of other data into the int.
 * One bit is lost
 * to support features extending past the extent on the right edge of the tile.
 * This leaves us with 2^13 = 8192
 */
//constexpr int32_t EXTENT = 8192; // TODO:

//constexpr float tileSize = 512; // TODO:

int LineStringFiller::insertVertex(size_t index, enum ngsBufferType indexType)
{
    // Based on the maxbox's LineBucket::addGeometry() from
    // https://github.com/mapbox/mapbox-gl-native

    // TODO: add getZ + level

    size_t vertexIndex =
            m_currBuffer->getVertexBufferSize() / VERTEX_WITH_NORMAL_SIZE;

    if (m_closed && index == m_numPoints - 1) {
        // if the line is closed, we treat the last vertex like the first
        m_line->getPoint(1, &m_nextPt);
    } else if (index + 1 < m_numPoints) {
        // just the next vertex
        m_line->getPoint(index + 1, &m_nextPt);
    } else {
        // there is no next vertex
        m_nextPt.empty();
    }

    if (m_nextNormal) {
        m_prevNormal = m_nextNormal;
    }
    if (m_currPt) {
        m_prevPt = m_currPt;
    }

    m_line->getPoint(index, &m_currPt);

    // if two consecutive vertices exist, skip the current one
    if (m_nextPt && m_currPt == m_nextPt) {
        return -1;
    }

    // Calculate the normal towards the next vertex in this line. In case
    // there is no next vertex, pretend that the line is continuing
    // straight, meaning that we are just using the previous normal.
    m_nextNormal = m_nextPt ? m_nextPt.normal(m_currPt) : m_prevNormal;

    // If we still don't have a previous normal, this is the beginning of a
    // non-closed line, so we're doing a straight "join".
    if (!m_prevNormal) {
        m_prevNormal = m_nextNormal;
    }

    // Determine the normal of the join extrusion. It is the angle bisector
    // of the segments between the previous line and the next line.
    Vector2 joinNormal = (m_prevNormal + m_nextNormal).unit();

    /*  joinNormal     prevNormal
     *             ↖      ↑
     *                .________. prevVertex
     *                |
     * nextNormal  ←  |  currentVertex
     *                |
     *     nextVertex !
     *
     */

    // Calculate the length of the miter.
    // Find the cosine of the angle between the next and join normals.
    // The inverse of that is the miter length.
    const double cosHalfAngle = joinNormal.getX() * m_nextNormal.getX()
            + joinNormal.getY() * m_nextNormal.getY();
    const double miterLength = cosHalfAngle != 0 ? 1 / cosHalfAngle : 1;

    //    const bool isSharpCorner = // TODO:
    //            (cosHalfAngle < COS_HALF_SHARP_CORNER) && prevPt && nextPt;

    //    if (isSharpCorner && i > 0) { // TODO:
    //        const double prevSegmentLength = currPt.dist(prevPt);
    //        if (prevSegmentLength > 2.0 * sharpCornerOffset) {
    //            Vector2 newPrevPt = currPt
    //                    - (currPt - prevPt)
    //                            * (sharpCornerOffset / prevSegmentLength);
    //            addCurrentLineVertex(newPrevPt, level, prevNormal, 0, 0, false,
    //                    startIndex, currBuffer);
    //            prevPt = newPrevPt;
    //        }
    //    }

    // The join if a middle vertex, otherwise the cap
    const bool middleVertex = m_prevPt && m_nextPt;
    LineJoinType currentJoin = m_layoutLineJoin;
    // TODO:
    /*const*/ LineCapType currentCap = m_nextPt ? m_beginCap : m_endCap;

    if (middleVertex) {
        if (currentJoin == LineJoinType::Round) {  // TODO:
            if (miterLength < 1.05 /* layout.get<LineRoundLimit>() */) {
                currentJoin = LineJoinType::Miter;
            } else /*if (miterLength <= 2)*/ {  // TODO:
                currentJoin = LineJoinType::FakeRound;
            }
        }

        if (currentJoin == LineJoinType::Miter && miterLength > m_miterLimit) {
            currentJoin = LineJoinType::Bevel;
        }

        if (currentJoin == LineJoinType::Bevel) {
            // The maximum extrude length is 128 / 63 = 2 times the width
            // of the line so if miterLength >= 2 we need to draw
            // a different type of bevel where.
            if (miterLength > 2) {
                currentJoin = LineJoinType::FlipBevel;
            }

            // If the miterLength is really small and the line bevel
            // wouldn't be visible, just draw a miter join
            // to save a triangle.
            if (miterLength < m_miterLimit) {
                currentJoin = LineJoinType::Miter;
            }
        }
    } else {
        if (currentCap == LineCapType::Round) {  // TODO:
            currentCap = LineCapType::FakeRound;
        }
    }

    if (middleVertex && currentJoin == LineJoinType::Miter) {
        joinNormal = joinNormal * miterLength;
        addCurrentLineVertex(m_currPt, m_level, joinNormal, 0, 0, false,
                m_currBuffer, indexType);

    } else if (middleVertex && currentJoin == LineJoinType::FlipBevel) {
        // miter is too big, flip the direction to make a beveled join

        if (miterLength > 100) {
            // Almost parallel lines
            joinNormal = m_nextNormal;
        } else {
            const double direction =
                    m_prevNormal.crossProduct(m_nextNormal) > 0 ? -1 : 1;
            const double bevelLength = miterLength
                    * (m_prevNormal + m_nextNormal).magnitude()
                    / (m_prevNormal - m_nextNormal).magnitude();
            joinNormal = joinNormal.cross() * bevelLength * direction;
        }

        addCurrentLineVertex(m_currPt, m_level, joinNormal, 0, 0, false,
                m_currBuffer, indexType);

        addCurrentLineVertex(m_currPt, m_level, joinNormal * -1.0, 0, 0, false,
                m_currBuffer, indexType);

    } else if (middleVertex
            && (currentJoin == LineJoinType::Bevel
                       || currentJoin == LineJoinType::FakeRound)) {
        const bool lineTurnsLeft = m_prevNormal.crossProduct(m_nextNormal) > 0;
        const float offset = -std::sqrt(miterLength * miterLength - 1);
        float offsetA;
        float offsetB;

        if (lineTurnsLeft) {
            offsetB = 0;
            offsetA = offset;
        } else {
            offsetA = 0;
            offsetB = offset;
        }

        // Close previous segment with bevel
        if (!m_startOfLine) {
            addCurrentLineVertex(m_currPt, m_level, m_prevNormal, offsetA,
                    offsetB, false, m_currBuffer, indexType);
        }

        if (currentJoin == LineJoinType::FakeRound) {
            // The join angle is sharp enough that a round join would be
            // visible. Bevel joins fill the gap between segments with a
            // single pie slice triangle. Create a round join by adding
            // multiple pie slices. The join isn't actually round, but
            // it looks like it is at the sizes we render lines at.

            // Add more triangles for sharper angles.
            // This math is just a good enough approximation.
            // It isn't "correct".
            const int n = std::floor((0.5 - (cosHalfAngle - 0.5)) * 8);

            for (int m = 0; m < n; ++m) {
                Vector2 approxFractionalJoinNormal =
                        (m_nextNormal * ((m + 1.0) / (n + 1.0)) + m_prevNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, lineTurnsLeft, false,
                        m_currBuffer, indexType);
            }

            addPieSliceLineVertex(m_currPt, m_level, joinNormal, lineTurnsLeft,
                    false, m_currBuffer, indexType);

            for (int k = n - 1; k >= 0; --k) {
                Vector2 approxFractionalJoinNormal =
                        (m_prevNormal * ((k + 1.0) / (n + 1.0)) + m_nextNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, lineTurnsLeft, false,
                        m_currBuffer, indexType);
            }
        }

        // Start next segment
        if (m_nextPt) {
            addCurrentLineVertex(m_currPt, m_level, m_nextNormal, -offsetA,
                    -offsetB, false, m_currBuffer, indexType);
        }

    } else if (!middleVertex && currentCap == LineCapType::Butt) {
        if (!m_startOfLine) {
            // Close previous segment with a butt
            addCurrentLineVertex(m_currPt, m_level, m_prevNormal, 0, 0, false,
                    m_currBuffer, indexType);
        }

        // Start next segment with a butt
        if (m_nextPt) {
            addCurrentLineVertex(m_currPt, m_level, m_nextNormal, 0, 0, false,
                    m_currBuffer, indexType);
        }

    } else if (!middleVertex && currentCap == LineCapType::Square) {
        if (!m_startOfLine) {
            // Close previous segment with a square cap
            addCurrentLineVertex(m_currPt, m_level, m_prevNormal, 1, 1, false,
                    m_currBuffer, indexType);

            // The segment is done. Unset vertices to disconnect segments.
            m_e1 = m_e2 = -1;
        }

        // Start next segment
        if (m_nextPt) {
            addCurrentLineVertex(m_currPt, m_level, m_nextNormal, -1, -1, false,
                    m_currBuffer, indexType);
        }

    } else if (middleVertex ? currentJoin == LineJoinType::Round
                            : currentCap == LineCapType::Round) {
        if (!m_startOfLine) {
            // Close previous segment with a butt
            addCurrentLineVertex(m_currPt, m_level, m_prevNormal, 0, 0, false,
                    m_currBuffer, indexType);

            // Add round cap or linejoin at end of segment
            addCurrentLineVertex(m_currPt, m_level, m_prevNormal, 1, 1, true,
                    m_currBuffer, indexType);

            // The segment is done. Unset vertices to disconnect segments.
            m_e1 = m_e2 = -1;
        }

        // Start next segment with a butt
        if (m_nextPt) {
            // Add round cap before first segment
            addCurrentLineVertex(m_currPt, m_level, m_nextNormal, -1, -1, true,
                    m_currBuffer, indexType);

            addCurrentLineVertex(m_currPt, m_level, m_nextNormal, 0, 0, false,
                    m_currBuffer, indexType);
        }

    } else if (!middleVertex && currentCap == LineCapType::FakeRound) {
        // TODO: Remove work with LineCapType::FakeRound and switch
        // to LineCapType::Round based on the antialiased color changing
        // for LineCapType::Square.

        // Fill the fake round cap with a single pie slice triangle.
        // Create a fake round cap by adding multiple pie slices.
        // The cap isn't actually round, but it looks like it is
        // at the sizes we render lines at.

        // Add more triangles for fake round cap.
        // This math is just a good enough approximation.
        // It isn't "correct".
        const int n = 4;

        if (!m_startOfLine) {
            // Close previous segment with a butt
            addCurrentLineVertex(m_currPt, m_level, m_prevNormal, 0, 0, false,
                    m_currBuffer, indexType);

            // Add fake round cap at end of segment
            Vector2 invNormal = m_prevNormal * -1;
            Vector2 crossNormal = invNormal.cross();

            for (int m = 0; m < n; ++m) {
                Vector2 approxFractionalJoinNormal =
                        (crossNormal * ((m + 1.0) / (n + 1.0)) + m_prevNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, false, false, m_currBuffer,
                        indexType);
            }

            for (int k = n - 1; k >= 0; --k) {
                Vector2 approxFractionalJoinNormal =
                        (m_prevNormal * ((k + 1.0) / (n + 1.0)) + crossNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, false, false, m_currBuffer,
                        indexType);
            }

            addPieSliceLineVertex(m_currPt, m_level, crossNormal, false, false,
                    m_currBuffer, indexType);

            for (int m = 0; m < n; ++m) {
                Vector2 approxFractionalJoinNormal =
                        (invNormal * ((m + 1.0) / (n + 1.0)) + crossNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, false, false, m_currBuffer,
                        indexType);
            }

            for (int k = n - 1; k >= 0; --k) {
                Vector2 approxFractionalJoinNormal =
                        (crossNormal * ((k + 1.0) / (n + 1.0)) + invNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, false, false, m_currBuffer,
                        indexType);
            }

            // The segment is done. Unset vertices to disconnect segments.
            m_e1 = m_e2 = -1;
        }

        if (m_nextPt) {
            // Add fake round cap before first segment
            Vector2 invNormal = m_nextNormal * -1;
            Vector2 crossNormal = m_nextNormal.cross();

            bool firstPt = true;
            for (int m = 0; m < n; ++m) {
                Vector2 approxFractionalJoinNormal =
                        (crossNormal * ((m + 1.0) / (n + 1.0)) + invNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, false, firstPt,
                        m_currBuffer, indexType);
                if (firstPt)
                    firstPt = false;
            }

            for (int k = n - 1; k >= 0; --k) {
                Vector2 approxFractionalJoinNormal =
                        (invNormal * ((k + 1.0) / (n + 1.0)) + crossNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, false, false, m_currBuffer,
                        indexType);
            }

            addPieSliceLineVertex(m_currPt, m_level, crossNormal, false, false,
                    m_currBuffer, indexType);

            for (int m = 0; m < n; ++m) {
                Vector2 approxFractionalJoinNormal =
                        (m_nextNormal * ((m + 1.0) / (n + 1.0)) + crossNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, false, false, m_currBuffer,
                        indexType);
            }

            for (int k = n - 1; k >= 0; --k) {
                Vector2 approxFractionalJoinNormal =
                        (crossNormal * ((k + 1.0) / (n + 1.0)) + m_nextNormal)
                                .unit();
                addPieSliceLineVertex(m_currPt, m_level,
                        approxFractionalJoinNormal, false, false, m_currBuffer,
                        indexType);
            }

            // Start next segment with a butt
            addCurrentLineVertex(m_currPt, m_level, m_nextNormal, 0, 0, false,
                    m_currBuffer, indexType);
        }
    }

    //        if (isSharpCorner && i < numPoints - 1) { // TODO:
    //            const double nextSegmentLength = currPt.dist(nextPt);
    //            if (nextSegmentLength > 2 * sharpCornerOffset) {
    //                Vector2 newCurrPt = currPt
    //                        + (nextPt - currPt)
    //                                * (sharpCornerOffset / nextSegmentLength);
    //                addCurrentLineVertex(newCurrPt, m_level, nextNormal, 0, 0, false,
    //                        startIndex, currBuffer);
    //                currPt = newCurrPt;
    //            }
    //        }

    if (m_startOfLine)
        m_startOfLine = false;

    return vertexIndex;
}

/*
void LineStringFiller::addCurrentLineVertex(const Vector2& currPt,
        float level,
        const Vector2& normal,
        double endLeft,
        double endRight,
        bool round,
        GlBufferSharedPtr currBuffer,
        enum ngsBufferType indexType)
{
    // Add point coordinates as float.
    // Add triangle indices as unsigned short.

    float ptx =
            static_cast<float>(currPt.getX() + m_crossExtent * DEFAULT_MAX_X2);
    float pty = static_cast<float>(currPt.getY());
    float ptz = level;

    Vector2 extrude = normal;
    if (endLeft)
        extrude = extrude - (normal.cross() * endLeft);

    // v(i*2), extrude
    float ex = static_cast<float>(extrude.getX());
    float ey = static_cast<float>(extrude.getY());
    currBuffer->addVertex(ptx, pty, ptz, true, ex, ey);

    m_e3 = currBuffer->getVertexBufferSize() / VERTEX_WITH_NORMAL_SIZE - 1;
    if (m_e1 >= 0 && m_e2 >= 0) {
        currBuffer->addTriangleIndices(m_e1, m_e2, m_e3, indexType);
    }
    m_e1 = m_e2;
    m_e2 = m_e3;


    extrude = normal * -1.0;
    if (endRight)
        extrude = extrude - (normal.cross() * endRight);

    // v(i*2+1), -extrude
    ex = static_cast<float>(extrude.getX());
    ey = static_cast<float>(extrude.getY());
    currBuffer->addVertex(ptx, pty, ptz, true, ex, ey);

    m_e3 = currBuffer->getVertexBufferSize() / VERTEX_WITH_NORMAL_SIZE - 1;
    if (m_e1 >= 0 && m_e2 >= 0) {
        currBuffer->addTriangleIndices(m_e1, m_e2, m_e3, indexType);
    }
    m_e1 = m_e2;
    m_e2 = m_e3;
}

void LineStringFiller::addPieSliceLineVertex(const Vector2& currPt,
        float level,
        const Vector2& extrude,
        bool lineTurnsLeft,
        bool firstPt,
        GlBufferSharedPtr currBuffer,
        enum ngsBufferType indexType)
{
    // Add point coordinates as float.
    // Add triangle indices as unsigned short.

    Vector2 flippedExtrude = extrude * (lineTurnsLeft ? -1.0 : 1.0);
    float ptx =
            static_cast<float>(currPt.getX() + m_crossExtent * DEFAULT_MAX_X2);
    float pty = static_cast<float>(currPt.getY());
    float ptz = level;

    float fex = static_cast<float>(flippedExtrude.getX());
    float fey = static_cast<float>(flippedExtrude.getY());
    currBuffer->addVertex(ptx, pty, ptz, true, fex, fey);

    m_e3 = currBuffer->getVertexBufferSize() / VERTEX_WITH_NORMAL_SIZE - 1;
    if (m_e1 >= 0 && m_e2 >= 0) {
        currBuffer->addTriangleIndices(m_e1, m_e2, m_e3, indexType);
    }

    if (lineTurnsLeft) {
        if (firstPt) {
            m_e1 = m_e3;
        }
        m_e2 = m_e3;
    } else {
        if (firstPt) {
            m_e2 = m_e3;
        }
        m_e1 = m_e3;
    }
}

PolygonTriangulator::PolygonTriangulator()
        : m_numTriangles(0)
        , m_numVertices(0)
{
}

PolygonTriangulator::~PolygonTriangulator()
{
}

void PolygonTriangulator::markDomains(CDT& ct,
        CDT::Face_handle start,
        int index,
        std::list<CDT::Edge>& border)
{
    if (start->info().m_nestingLevel != -1) {
        return;
    }
    std::list<CDT::Face_handle> queue;
    queue.push_back(start);
    while (!queue.empty()) {
        CDT::Face_handle fh = queue.front();
        queue.pop_front();
        if (fh->info().m_nestingLevel == -1) {
            fh->info().m_nestingLevel = index;
            for (int i = 0; i < 3; i++) {
                CDT::Edge e(fh, i);
                CDT::Face_handle n = fh->neighbor(i);
                if (n->info().m_nestingLevel == -1) {
                    if (ct.is_constrained(e))
                        border.push_back(e);
                    else
                        queue.push_back(n);
                }
            }
        }
    }
}

// Explore set of facets connected with non constrained edges,
// and attribute to each such set a nesting level.
// We start from facets incident to the infinite vertex, with a nesting
// level of 0. Then we recursively consider the non-explored facets incident
// to constrained edges bounding the former set
// and increase the nesting level by 1.
// Facets in the domain are those with an odd nesting level.
void PolygonTriangulator::markDomains(CDT& cdt)
{
    std::list<CDT::Edge> border;
    markDomains(cdt, cdt.infinite_face(), 0, border);
    while (!border.empty()) {
        CDT::Edge e = border.front();
        border.pop_front();
        CDT::Face_handle n = e.first->neighbor(e.second);
        if (n->info().m_nestingLevel == -1) {
            markDomains(cdt, n, e.first->info().m_nestingLevel + 1, border);
        }
    }
}

size_t PolygonTriangulator::insertConstraint(
        const OGRLineString* line, const size_t startIndex)
{
    // Insert the line as constrains into a constrained triangulation.
    // Based on Constrained_Delaunay_triangulation_2::insert_constraint().
    // Work only for non-intersecting nested polygons/rings.
    // TODO: Make for intersecting nested polygons/rings (we need it?).

    int numPoints = line->getNumPoints();
    size_t index = startIndex;

    OGRPoint pt;
    line->getPoint(0, &pt);
    CDT::Point p0(pt.getX(), pt.getY());
    CDT::Point p = p0;

    CDT::Vertex_handle v0 = m_cdt.insert(p0);
    v0->info().m_index = index;
    ++index;

    CDT::Vertex_handle v(v0), w(v0);

    for (int i = 1; i < numPoints; ++i) {
        line->getPoint(i, &pt);
        const CDT::Point q(pt.getX(), pt.getY());
        if (p != q) {

            w = m_cdt.insert(q);
            w->info().m_index = index;
            ++index;

            m_cdt.insert_constraint(v, w);
            v = w;
            p = q;
        }
    }

    m_numVertices += (index - startIndex);
    return index;
}

void PolygonTriangulator::markDomains()
{
    // Mark facets that are inside the domain bounded by the polygon.
    markDomains(m_cdt);

    // Get count of triangles that are inside the domain and mark their vertices.
    for (CDT::Finite_faces_iterator fit = m_cdt.finite_faces_begin();
            fit != m_cdt.finite_faces_end(); ++fit) {

        if (fit->info().inDomain()) {
            for (int i = 0; i < 3; ++i) {
                // for additional vertices
                if (!fit->vertex(i)->info().inDomain()) {
                    fit->vertex(i)->info().setAsAdditionalVertexInDomain();
                    ++m_numVertices;
                }
            }
            ++m_numTriangles;
        }
    }
}

void PolygonTriangulator::insertVerticesToBuffer(float level,
        int_least8_t crossExtent,
        const GlBufferSharedPtr buffer,
        const std::vector<size_t>* borderIndices)
{
    // Get vertices of triangles, set their indices and copy additional to buffer.
    for (CDT::Finite_vertices_iterator vit = getCdt().finite_vertices_begin();
            vit != getCdt().finite_vertices_end(); ++vit) {

        if (vit->info().inDomain()) {
            if (borderIndices) {
                if (!vit->info().isAdditionalVertex()) {
                    vit->info().m_index = (*borderIndices)[vit->info().m_index];
                } else {
                    // Set index by buffer
                    vit->info().m_index = buffer->getVertexBufferSize()
                            / VERTEX_WITH_NORMAL_SIZE;

                    // Normal is not needed, but storage for normal must be occupied.
                    float x = static_cast<float>(
                            vit->point().x() + crossExtent * DEFAULT_MAX_X2);
                    float y = static_cast<float>(vit->point().y());
                    float z = level;
                    buffer->addVertex(x, y, z, true, 0, 0);
                }
            } else {
                float x = static_cast<float>(
                        vit->point().x() + crossExtent * DEFAULT_MAX_X2);
                float y = static_cast<float>(vit->point().y());
                float z = level;
                buffer->addVertex(x, y, z);

                // Set index by buffer
                vit->info().m_index =
                        buffer->getVertexBufferSize() / VERTEX_SIZE - 1;
            }
        }
    }

    // Get indices of vertices and copy them to border index buffer.
    for (CDT::Finite_faces_iterator fit = getCdt().finite_faces_begin();
            fit != getCdt().finite_faces_end(); ++fit) {

        if (fit->info().inDomain()) {
            int i0 = fit->vertex(0)->info().m_index;
            int i1 = fit->vertex(1)->info().m_index;
            int i2 = fit->vertex(2)->info().m_index;
            buffer->addTriangleIndices(i0, i1, i2);
        }
    }
}

void PolygonTriangulator::triangulate(const OGRPolygon* polygon)
{
    const int intRingCnt = polygon->getNumInteriorRings();
    for (int i = -1; i < intRingCnt; ++i) {
        const OGRLinearRing* ring;
        if (-1 == i) {
            ring = polygon->getExteriorRing();
        } else {
            ring = polygon->getInteriorRing(i);
        }

        int numPoints = ring->getNumPoints();
        if (numPoints < 3) {
            if (-1 == i)
                return;
            else
                continue;
        }

        // TODO: cut line by x or y direction or
        // tesselate to fill into array max size
        if (numPoints > 21000) {
            CPLDebug("GlBufferBucket", "Too many points - %d, need to divide",
                    numPoints);
            return;
        }

        insertConstraint(ring);
    }

    // Mark facets that are inside the domain bounded by the polygon.
    markDomains();
}

*/
