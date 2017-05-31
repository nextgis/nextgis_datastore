/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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
#include "buffer.h"

namespace ngs {

constexpr GLuint GL_BUFFER_IVALID = 0;
constexpr unsigned short MAX_INDEX_BUFFER_SIZE = 65535;
constexpr unsigned char VERTEX_SIZE = 3;
// 5 = 3 for vertex + 2 for normal
constexpr unsigned char VERTEX_WITH_NORMAL_SIZE = 5;
//GL_UNSIGNED_BYTE, with a maximum value of 255.
//GL_UNSIGNED_SHORT, with a maximum value of 65,535
constexpr unsigned short MAX_VERTEX_BUFFER_SIZE = 65535;

GlBuffer::GlBuffer(BufferType type) : GlObject(),
    m_bufferIds{{GL_BUFFER_IVALID,GL_BUFFER_IVALID}},
    m_type(type)
{
    m_vertices.reserve(MAX_VERTEX_BUFFER_SIZE);
    m_indices.reserve(MAX_INDEX_BUFFER_SIZE);
}

GlBuffer::~GlBuffer()
{
    // NOTE: Delete buffer must be in GL context
}

bool GlBuffer::canStoreVertices(size_t amount, bool withNormals) const
{
    return (m_vertices.size() + amount * (withNormals ? VERTEX_WITH_NORMAL_SIZE :
                                             VERTEX_SIZE)) < MAX_VERTEX_BUFFER_SIZE;
}

void GlBuffer::destroy()
{
    if (m_bound) {
        ngsCheckGLError(glDeleteBuffers(GL_BUFFERS_COUNT, m_bufferIds.data()));
    }
}

GLuint GlBuffer::id(bool vertices) const
{
    if(vertices)
        return m_bufferIds[0];
    else
        return m_bufferIds[1];
}

size_t GlBuffer::maxIndexes()
{
    return MAX_INDEX_BUFFER_SIZE;
}

size_t GlBuffer::maxVertices()
{
    return MAX_VERTEX_BUFFER_SIZE;
}

void GlBuffer::bind()
{
    if (m_bound || m_vertices.empty() || m_indices.empty())
        return;

    ngsCheckGLError(glGenBuffers(GL_BUFFERS_COUNT, m_bufferIds.data()));

    ngsCheckGLError(glBindBuffer(GL_ARRAY_BUFFER, id(true)));
    GLsizeiptr size = static_cast<GLsizeiptr>(sizeof(GLfloat) * m_vertices.size());
    ngsCheckGLError(glBufferData(GL_ARRAY_BUFFER, size, m_vertices.data(),
            GL_STATIC_DRAW));

    ngsCheckGLError(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id(false)));
    size = static_cast<GLsizeiptr>(sizeof(GLushort) * m_indices.size());
    ngsCheckGLError(glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, m_indices.data(),
            GL_STATIC_DRAW));
    m_bound = true;
}

void GlBuffer::rebind() const
{
    ngsCheckGLError(glBindBuffer(GL_ARRAY_BUFFER, id(true)));
    ngsCheckGLError(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id(false)));

}

} // namespace ngs


/*

void GlBufferBucket::fill(const OGRGeometry* geom, float level)
{
    switch (OGR_GT_Flatten(geom->getGeometryType())) {
        case wkbPoint: {
            const OGRPoint* pt = static_cast<const OGRPoint*>(geom);
            fillPoint(pt, level);
        } break;
        case wkbLineString: {
            const OGRLineString* line = static_cast<const OGRLineString*>(geom);
            fillLineString(line, level);
        } break;
        case wkbPolygon: {
            const OGRPolygon* polygon = static_cast<const OGRPolygon*>(geom);
//            fillPolygon(polygon, level);
            fillBorderedPolygon(polygon, level);
        } break;
        case wkbMultiPoint: {
            const OGRMultiPoint* mpt = static_cast<const OGRMultiPoint*>(geom);
            for (int i = 0; i < mpt->getNumGeometries(); ++i) {
                const OGRPoint* pt =
                        static_cast<const OGRPoint*>(mpt->getGeometryRef(i));
                fillPoint(pt, level);
            }
        } break;
        case wkbMultiLineString: {
            const OGRMultiLineString* mln =
                    static_cast<const OGRMultiLineString*>(geom);
            for (int i = 0; i < mln->getNumGeometries(); ++i) {
                const OGRLineString* line = static_cast<const OGRLineString*>(
                        mln->getGeometryRef(i));
                fillLineString(line, level);
            }
        } break;
        case wkbMultiPolygon: {
            const OGRMultiPolygon* mplg =
                    static_cast<const OGRMultiPolygon*>(geom);
            for (int i = 0; i < mplg->getNumGeometries(); ++i) {
                const OGRPolygon* polygon =
                        static_cast<const OGRPolygon*>(mplg->getGeometryRef(i));
//                fillPolygon(polygon, level);
                fillBorderedPolygon(polygon, level);
            }
        } break;
        case wkbGeometryCollection: {
            const OGRGeometryCollection* coll =
                    static_cast<const OGRGeometryCollection*>(geom);
            for (int i = 0; i < coll->getNumGeometries(); ++i) {
                fill(coll->getGeometryRef(i), level);
            }
        } break;
            /*
        case wkbCircularString:
            return "cir";
        case wkbCompoundCurve:
            return "ccrv";
        case wkbCurvePolygon:
            return "crvplg";
        case wkbMultiCurve:
            return "mcrv";
        case wkbMultiSurface:
            return "msurf";
        case wkbCurve:
            return "crv";
        case wkbSurface:
            return "surf";*//*
    }
}

/*

void GlBufferBucket::fillPolygon(const OGRPolygon* polygon, float level)
{
    PolygonTriangulator tr;
    tr.triangulate(polygon);

    if (!m_buffers.back()->canStoreVertices(tr.getNumVertices())
            || !m_buffers.back()->canStoreIndices(3 * tr.getNumTriangles())) {
        if (!GlBuffer::canGlobalStoreVertices(tr.getNumVertices())
                || !GlBuffer::canGlobalStoreIndices(3 * tr.getNumTriangles())) {
            return;
        }
        m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
    }

    GlBufferSharedPtr currBuffer = m_buffers.back();
    tr.insertVerticesToBuffer(level, m_crossExtent, currBuffer);
}

void GlBufferBucket::fillBorderedPolygon(const OGRPolygon* polygon, float level)
{
    PolygonTriangulator tr;
    size_t polygonPointIndex = 0;
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

        polygonPointIndex = tr.insertConstraint(ring, polygonPointIndex);
    }

    // Mark facets that are inside the domain bounded by the polygon.
    tr.markDomains();

    int maxVerticesCnt = std::max(2 * polygonPointIndex, tr.getNumVertices());
    int maxIndicesCnt =
            std::max(6 * (polygonPointIndex - 1), 3 * tr.getNumTriangles());

    if (!m_buffers.back()->canStoreVertices(maxVerticesCnt, true)
            || !m_buffers.back()->canStoreIndices(maxIndicesCnt)) {
        if (!GlBuffer::canGlobalStoreVertices(maxVerticesCnt, true)
                || !GlBuffer::canGlobalStoreIndices(maxIndicesCnt)) {
            return;
        }
        m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
    }

    GlBufferSharedPtr currBuffer = m_buffers.back();
    std::vector<size_t> borderIndices(polygonPointIndex);
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

        LineStringFiller lf(ring, level, m_crossExtent, LineCapType::Butt,
                LineJoinType::Bevel, currBuffer);
        for (int i = 0; i < numPoints; ++i) {
            int vertexIndex = lf.insertVertex(i, BF_BORDER_INDICES);
            if (-1 == vertexIndex) {

            }
            borderIndices.push_back(vertexIndex);
        }
    }

    tr.insertVerticesToBuffer(level, m_crossExtent, currBuffer, &borderIndices);
}

*/
