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

GlBuffer::GlBuffer() : GlObject(),
    m_bufferIds{{GL_BUFFER_IVALID,GL_BUFFER_IVALID,GL_BUFFER_IVALID}}
{
    m_vertices.reserve(MAX_VERTEX_BUFFER_SIZE);
    m_indices.reserve(MAX_INDEX_BUFFER_SIZE);
    m_borderIndices.reserve(MAX_INDEX_BUFFER_SIZE);
    // TODO: Create struct of FID, indices, borderIndices
    // Form m_indices from indices
    // Form m_borderIndices from borderIndices
    // Delete FID
    // Pack arrays on save
    // Remove FID from drawing
}

GlBuffer::~GlBuffer()
{
// FIXME: Delete buffer must be in GL context
//    destroy();
}


void GlBuffer::destroy()
{
    if (m_bound) {
        ngsCheckGLError(glDeleteBuffers(GL_BUFFERS_COUNT, m_bufferIds.data()));
    }
}

GLuint GlBuffer::getGlBufferId(GlBuffer::BufferType type) const
{
    switch (type) {
        case BF_VERTICES:
            return m_bufferIds[0];
        case BF_INDICES:
            return m_bufferIds[1];
        case BF_BORDER_INDICES:
            return m_bufferIds[2];
    }
    return GL_BUFFER_IVALID;
}

void GlBuffer::bind()
{
    if (m_bound || m_vertices.empty() || m_indices.empty())
        return;

    ngsCheckGLError(glGenBuffers(GL_BUFFERS_COUNT, m_bufferIds.data()));

    ngsCheckGLError(glBindBuffer(GL_ARRAY_BUFFER, getGlBufferId(BF_VERTICES)));
    GLsizeiptr size = static_cast<GLsizeiptr>(sizeof(GLfloat) * m_vertices.size());
    ngsCheckGLError(glBufferData(GL_ARRAY_BUFFER, size, m_vertices.data(),
            GL_STATIC_DRAW));

    ngsCheckGLError(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, getGlBufferId(BF_INDICES)));
    size = static_cast<GLsizeiptr>(sizeof(GLushort) * m_indices.size());
    ngsCheckGLError(glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, m_indices.data(),
            GL_STATIC_DRAW));

    if (!m_borderIndices.empty()) {
        ngsCheckGLError(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                     getGlBufferId(BF_BORDER_INDICES)));
        size = static_cast<GLsizeiptr>(sizeof(GLushort) * m_borderIndices.size());
        ngsCheckGLError(glBufferData(GL_ELEMENT_ARRAY_BUFFER, size,
                m_borderIndices.data(), GL_STATIC_DRAW));
        m_borderIndices.clear();
    }
    m_bound = true;
}


} // namespace ngs


/*

//------------------------------------------------------------------------------
// GlBuffer
//------------------------------------------------------------------------------

// static
bool GlBuffer::canGlobalStoreVertices(size_t amount, bool withNormals)
{
    return (m_globalVertexBufferSize.load()
                   + amount * (withNormals ? VERTEX_WITH_NORMAL_SIZE
                                           : VERTEX_SIZE))
            < MAX_GLOBAL_VERTEX_BUFFER_SIZE;
}

// static
bool GlBuffer::canGlobalStoreIndices(
        size_t amount, enum ngsBufferType indexType)
{
    int_fast32_t size;
    switch (indexType) {
        case BF_INDICES:
            size = m_globalIndexBufferSize.load();
            break;
        case BF_BORDER_INDICES:
            size = m_globalBorderIndexBufferSize.load();
            break;
        default:
            return false;
    }

    return (size + amount) < MAX_GLOBAL_INDEX_BUFFER_SIZE;
}

bool GlBuffer::canStoreVertices(size_t amount, bool withNormals) const
{
    return ((m_vertices.size()
                    + amount * (withNormals ? VERTEX_WITH_NORMAL_SIZE
                                            : VERTEX_SIZE))
                   < MAX_VERTEX_BUFFER_SIZE)
            && canGlobalStoreVertices(amount, withNormals);
}

bool GlBuffer::canStoreIndices(
        size_t amount, enum ngsBufferType indexType) const
{
    int_fast32_t size;
    switch (indexType) {
        case BF_INDICES:
            size = m_indices.size();
            break;
        case BF_BORDER_INDICES:
            size = m_borderIndices.size();
            break;
        default:
            return false;
    }

    return ((size + amount) < MAX_INDEX_BUFFER_SIZE)
            && canGlobalStoreIndices(amount);
}

void GlBuffer::addVertex(
        float x, float y, float z, bool withNormals, float nX, float nY)
{
    if (!canStoreVertices(1, withNormals)) {
        return;
    }
    m_vertices.emplace_back(x);
    m_vertices.emplace_back(y);
    m_vertices.emplace_back(z);

    if (withNormals) {
        m_vertices.emplace_back(nX);
        m_vertices.emplace_back(nY);
    }

    m_globalVertexBufferSize.fetch_add(
            withNormals ? VERTEX_WITH_NORMAL_SIZE : VERTEX_SIZE);
}

void GlBuffer::addIndex(unsigned short index, enum ngsBufferType indexType)
{
    if (!canStoreIndices(1, indexType)) {
        return;
    }

    std::vector<GLushort>* pBuffer;
    std::atomic_int_fast32_t* pSize;

    switch (indexType) {
        case BF_INDICES:
            pBuffer = &m_indices;
            pSize = &m_globalIndexBufferSize;
            break;
        case BF_BORDER_INDICES:
            pBuffer = &m_borderIndices;
            pSize = &m_globalBorderIndexBufferSize;
            break;
        default:
            return;
    }

    pBuffer->emplace_back(index);
    pSize->fetch_add(1);
}

void GlBuffer::addTriangleIndices(unsigned short one,
        unsigned short two,
        unsigned short three,
        enum ngsBufferType indexType)
{
    if (!canStoreIndices(3, indexType)) {
        return;
    }

    std::vector<GLushort>* pBuffer;
    std::atomic_int_fast32_t* pSize;

    switch (indexType) {
        case BF_INDICES:
            pBuffer = &m_indices;
            pSize = &m_globalIndexBufferSize;
            break;
        case BF_BORDER_INDICES:
            pBuffer = &m_borderIndices;
            pSize = &m_globalBorderIndexBufferSize;
            break;
        default:
            return;
    }

    pBuffer->emplace_back(one);
    pBuffer->emplace_back(two);
    pBuffer->emplace_back(three);
    pSize->fetch_add(3);
}

size_t GlBuffer::getVertexBufferSize() const
{
    return m_bound ? m_finalVertexBufferSize : m_vertices.size();
}

size_t GlBuffer::getIndexBufferSize(enum ngsBufferType indexType) const
{
    switch (indexType) {
        case BF_INDICES:
            return m_bound ? m_finalIndexBufferSize : m_indices.size();
        case BF_BORDER_INDICES:
            return m_bound ? m_finalBorderIndexBufferSize
                           : m_borderIndices.size();
        default:
            return 0;
    }
}

//static
std::int_fast32_t GlBuffer::getGlobalVertexBufferSize()
{
    return m_globalVertexBufferSize.load();
}

//static
std::int_fast32_t GlBuffer::getGlobalIndexBufferSize(
        enum ngsBufferType indexType)
{
    switch (indexType) {
        case BF_INDICES:
            return m_globalIndexBufferSize.load();
        case BF_BORDER_INDICES:
            return m_globalBorderIndexBufferSize.load();
        default:
            return 0;
    }
}

//static
std::int_fast32_t GlBuffer::getGlobalHardBuffersCount()
{
    return m_globalHardBuffersCount.load();
}



//------------------------------------------------------------------------------
// GlBufferBucket
//------------------------------------------------------------------------------

GlBufferBucket::GlBufferBucket(int x,
        int y,
        unsigned char z,
        const OGREnvelope& env,
        char crossExtent)
        : m_X(x)
        , m_Y(y)
        , m_zoom(z)
        , m_extent(env)
        , m_filled(false)
        , m_crossExtent(crossExtent)
        , m_e1(-1)
        , m_e2(-1)
        , m_e3(-1)
{
    m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
}

GlBufferBucket::GlBufferBucket(GlBufferBucket&& other)
{
    *this = std::move(other);
}

GlBufferBucket::~GlBufferBucket()
{
    m_buffers.clear();
    m_buffers.shrink_to_fit();
}

GlBufferBucket& GlBufferBucket::operator=(GlBufferBucket&& other)
{
    if (this == &other) {
        return *this;
    }

    m_X = other.m_X;
    m_Y = other.m_Y;
    m_zoom = other.m_zoom;
    m_extent = other.m_extent;
    m_filled = other.m_filled;
    m_crossExtent = other.m_crossExtent;

    other.m_X = 0;
    other.m_Y = 0;
    other.m_zoom = 0;
    other.m_extent = OGREnvelope();
    other.m_filled = false;
    other.m_crossExtent = 0;

    m_buffers.clear();
    m_buffers.shrink_to_fit();
    m_buffers = std::move(other.m_buffers);

    m_fids.clear();
    m_fids = other.m_fids;
    other.m_fids.clear();

    return *this;
}

void GlBufferBucket::bind()
{
    for (const GlBufferSharedPtr& buff : m_buffers) {
        buff->bind();
    }
}

bool GlBufferBucket::bound() const
{
    for (const GlBufferSharedPtr& buff : m_buffers) {
        if (!buff->bound())
            return false;
    }
    return true;
}

bool GlBufferBucket::filled() const
{
    return m_filled;
}

void GlBufferBucket::setFilled(bool filled)
{
    m_filled = filled;
}

void GlBufferBucket::fill(GIntBig fid, OGRGeometry* geom, float level)
{
    if (nullptr == geom)
        return;
    std::int_fast32_t size = GlBuffer::getGlobalVertexBufferSize();
    fill(geom, level);
    if (0 < GlBuffer::getGlobalVertexBufferSize() - size) {
        m_fids.insert(fid);
    }
}

// TODO: add flags to specify how to fill buffer
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
            /* TODO:
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
void GlBufferBucket::fillPoint(const OGRPoint* point, float level)
{
    if (!m_buffers.back()->canStoreVertices(1)
            || !m_buffers.back()->canStoreIndices(1)) {
        if (!GlBuffer::canGlobalStoreVertices(1)
                || !GlBuffer::canGlobalStoreIndices(1)) {
            return;
        }
        m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
    }

    GlBufferSharedPtr currBuffer = m_buffers.back();
    unsigned short startIndex = currBuffer->getIndexBufferSize();
    // TODO: add getZ + level
    currBuffer->addVertex(
            static_cast<float>(point->getX() + m_crossExtent * DEFAULT_MAX_X2),
            static_cast<float>(point->getY()), level);

    currBuffer->addIndex(startIndex);
}

void GlBufferBucket::fillLineString(const OGRLineString* line, float level)
{
    int numPoints = line->getNumPoints();
    if (numPoints < 2)
        return;

    // TODO: cut line by x or y direction or
    // tesselate to fill into array max size
    if (numPoints > 21000) {
        CPLDebug("GlBufferBucket", "Too many points - %d, need to divide",
                numPoints);
        return;
    }

    if (!m_buffers.back()->canStoreVertices(2 * numPoints, true)
            || !m_buffers.back()->canStoreIndices(6 * (numPoints - 1))) {
        if (!GlBuffer::canGlobalStoreVertices(2 * numPoints, true)
                || !GlBuffer::canGlobalStoreIndices(6 * (numPoints - 1))) {
            return;
        }
        m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
    }

    GlBufferSharedPtr currBuffer = m_buffers.back();
    LineStringFiller lf(line, level, m_crossExtent, LineCapType::Butt,
            LineJoinType::Bevel, currBuffer);

    for (int i = 0; i < numPoints; ++i) {
        lf.insertVertex(i);
    }
}

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

        // TODO: cut line by x or y direction or
        // tesselate to fill into array max size
        if (numPoints > 21000) {
            CPLDebug("GlBufferBucket", "Too many points - %d, need to divide",
                    numPoints);
            return;
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
                // TODO:
            }
            borderIndices.push_back(vertexIndex);
        }
    }

    tr.insertVerticesToBuffer(level, m_crossExtent, currBuffer, &borderIndices);
}

char GlBufferBucket::crossExtent() const
{
    return m_crossExtent;
}

void GlBufferBucket::draw(const Style& style)
{
    for (const GlBufferSharedPtr& buffer : m_buffers) {
        if(!buffer->bound())
            buffer->bind();
        style.draw(*buffer);
    }
}

int GlBufferBucket::X() const
{
    return m_X;
}

int GlBufferBucket::Y() const
{
    return m_Y;
}

unsigned char GlBufferBucket::zoom() const
{
    return m_zoom;
}

void GlBufferBucket::free()
{
    m_buffers.clear ();
    m_buffers.emplace_back(makeSharedGlBuffer(GlBuffer()));
}

bool GlBufferBucket::hasFid(GIntBig fid) const
{
    return m_fids.find(fid) != m_fids.end();
}

int GlBufferBucket::getFidCount() const
{
    return m_fids.size();
}

OGREnvelope GlBufferBucket::extent() const
{
    return m_extent;
}

bool GlBufferBucket::intersects(const GlBufferBucket &other) const
{
    return m_extent.Intersects (other.m_extent) == TRUE;
}

bool GlBufferBucket::intersects(const OGREnvelope &ext) const
{
    return m_extent.Intersects (ext) == TRUE;
}

size_t GlBufferBucket::getVertexBufferSize() const
{
    size_t count = 0;
    for(const GlBufferSharedPtr& buff : m_buffers) {
        count += buff->getVertexBufferSize();
    }
    return count;
}

size_t GlBufferBucket::getIndexBufferSize() const
{
    size_t count = 0;
    for(const GlBufferSharedPtr& buff : m_buffers) {
        count += buff->getIndexBufferSize();
    }
    return count;
}

*/
