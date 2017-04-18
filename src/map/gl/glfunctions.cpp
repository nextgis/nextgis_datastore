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
#include "glfunctions.h"

#include "util/error.h"

/* Links:
//https://mkonrad.net/2014/12/08/android-off-screen-rendering-using-egl-pixelbuffers.html
//http://stackoverflow.com/questions/214437/opengl-fast-off-screen-rendering
//http://stackoverflow.com/questions/14785007/can-i-use-opengl-for-off-screen-rendering/14796456#14796456
//https://gist.github.com/CartBlanche/1271517
//http://stackoverflow.com/questions/21151259/replacing-glreadpixels-with-egl-khr-image-base-for-faster-pixel-copy
//https://vec.io/posts/faster-alternatives-to-glreadpixels-and-glteximage2d-in-opengl-es
//https://www.khronos.org/registry/egl/sdk/docs/man/html/eglIntro.xhtml
//https://wiki.maemo.org/SimpleGL_example
//http://stackoverflow.com/questions/12906971/difference-from-eglcreatepbuffersurface-and-eglcreatepixmapsurface-with-opengl-e
//http://stackoverflow.com/questions/25504188/is-it-possible-to-use-pixmaps-on-android-via-java-api-for-gles

//https://solarianprogrammer.com/2013/05/13/opengl-101-drawing-primitives/
//http://www.glprogramming.com/red/chapter02.html
//https://www3.ntu.edu.sg/home/ehchua/programming/opengl/CG_Introduction.html
//https://www3.ntu.edu.sg/home/ehchua/programming/android/Android_3D.html
//https://www.opengl.org/sdk/docs/man2/xhtml/gluUnProject.xml
//https://www.opengl.org/sdk/docs/man2/xhtml/gluProject.xml

//https://github.com/libmx3/mx3/blob/master/src/event_loop.cpp

//https://www.mapbox.com/blog/drawing-antialiased-lines/
//https://github.com/afiskon/cpp-opengl-vbo-vao-shaders/blob/master/main.cpp
*/

namespace ngs {

bool checkGLError(const char *cmd) {
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        const char *error = nullptr;
        switch (err) {
            case GL_INVALID_ENUM:
                error = "INVALID_ENUM";
            break;
            case GL_INVALID_VALUE:
                error = "INVALID_VALUE";
            break;
            case GL_INVALID_OPERATION:
                error = "INVALID_OPERATION";
            break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
            case GL_OUT_OF_MEMORY:
                error = "OUT_OF_MEMORY";
            break;
#ifdef GL_STACK_UNDERFLOW
            case GL_STACK_UNDERFLOW:
                error = "STACK_UNDERFLOW";
            break;
#endif
#ifdef GL_STACK_OVERFLOW
            case GL_STACK_OVERFLOW:
                error = "STACK_OVERFLOW";
            break;
#endif
            default:
                error = "(unknown)";
        }

        return !errorMessage("%s: Error GL_%s", cmd, error);
    }
    return false;
}

void reportGlStatus(GLuint obj) {
    GLint length;
    ngsCheckGLEerror(glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length));
    GLchar *log = new GLchar[length];
    ngsCheckGLEerror(glGetShaderInfoLog(obj, length, &length, log));
    warningMessage(log);
    delete [] log;
}

#ifdef USE_EGL
bool checkEGLError(const char *cmd) {
    EGLint err = eglGetError();
    if(err != EGL_SUCCESS){
        const char *error = nullptr;
        switch (err) {
        case EGL_NOT_INITIALIZED:
            error = "NOT_INITIALIZED";
            break;
        case EGL_BAD_ACCESS:
            error = "BAD_ACCESS";
        break;
        case EGL_BAD_ALLOC:
            error = "BAD_ALLOC";
            break;
        case EGL_BAD_ATTRIBUTE:
            error = "BAD_ATTRIBUTE";
            break;
        case EGL_BAD_CONTEXT:
            error = "BAD_CONTEXT";
            break;
        case EGL_BAD_CONFIG:
            error = "BAD_CONFIG";
            break;
        case EGL_BAD_CURRENT_SURFACE:
            error = "BAD_CURRENT_SURFACE";
            break;
        case EGL_BAD_DISPLAY:
            error = "BAD_DISPLAY";
            break;
        case EGL_BAD_SURFACE:
            error = "BAD_SURFACE";
            break;
        case EGL_BAD_MATCH:
            error = "BAD_MATCH";
            break;
        case EGL_BAD_PARAMETER:
            error = "BAD_PARAMETER";
            break;
        case EGL_BAD_NATIVE_PIXMAP:
            error = "BAD_NATIVE_PIXMAP";
            break;
        case EGL_BAD_NATIVE_WINDOW:
            error = "BAD_NATIVE_WINDOW";
            break;
        case EGL_CONTEXT_LOST:
            error = "CONTEXT_LOST";
            break;
        }
        return !errorMessage("%s: Error EGL_%s", cmd, error);
    }
    return false;
}
#endif // USE_EGL


} // namespace ngs


/*
#include <algorithm>
#include "cpl_error.h"
#include "cpl_string.h"
#include "glview.h"
#include <iostream>
#include "map/glfillers.h"
#include "style.h"
#include "ngstore/util/constants.h"

#define MAX_VERTEX_BUFFER_SIZE 16383
#define MAX_INDEX_BUFFER_SIZE 16383
#define MAX_GLOBAL_VERTEX_BUFFER_SIZE 327180000
#define MAX_GLOBAL_INDEX_BUFFER_SIZE 327180000


//------------------------------------------------------------------------------
// GlFuctions
//------------------------------------------------------------------------------

GlFuctions::GlFuctions() : m_extensionLoad(false), m_pBkChanged(true)
{
}

GlFuctions::~GlFuctions()
{
    // TODO: unload extensions
}

bool GlFuctions::init()
{
    if(!loadExtensions ())
        return false;
    return true;
}

bool GlFuctions::isOk() const
{
    return m_extensionLoad;
}

void GlFuctions::testDrawPreserved(int colorId) const
{
    static bool isBuffersFilled = false;
    static GLuint    buffers[2];
    if(!isBuffersFilled) {
        isBuffersFilled = true;
        ngsCheckGLEerror(glGenBuffers(2, buffers)); // TODO: glDeleteBuffers

        ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));

        GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                               -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                                4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                              };

        ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices), vVertices, GL_STATIC_DRAW));

        ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));

        GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                               -2236992.0f, 3972353.0f, 0.5f,
                                5187591.0f, 4509961.0f, 0.5f
                              };

        ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices2), vVertices2, GL_STATIC_DRAW));
    }

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));

    ngsCheckGLEerror(glUniform4f(colorId, 1.0f, 0.0f, 0.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));

    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));

    ngsCheckGLEerror(glUniform4f(colorId, 0.0f, 0.0f, 1.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));

    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

}

void GlFuctions::testDraw(int colorId) const
{
    GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                          };

    ngsCheckGLEerror(glUniform4f(colorId, 1.0f, 0.0f, 0.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                           -2236992.0f, 3972353.0f, 0.5f,
                            5187591.0f, 4509961.0f, 0.5f
                          };

    ngsCheckGLEerror(glUniform4f(colorId, 0.0f, 0.0f, 1.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices2 ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));
}

void GlFuctions::drawPolygons(const vector<GLfloat> &vertices,
                              const vector<GLushort> &indices) const
{
    if(vertices.empty() || indices.empty ())
        return;
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));
    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                            vertices.data ()));
    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, indices.size (),
                                    GL_UNSIGNED_SHORT, indices.data ()));
}

bool GlFuctions::loadExtensions()
{
    m_extensionLoad = true;
    return true;
}

//------------------------------------------------------------------------------
// GlBuffer
//------------------------------------------------------------------------------

// static
std::atomic_int_fast32_t GlBuffer::m_globalVertexBufferSize;
// static
std::atomic_int_fast32_t GlBuffer::m_globalIndexBufferSize;
// static
std::atomic_int_fast32_t GlBuffer::m_globalBorderIndexBufferSize;
// static
std::atomic_int_fast32_t GlBuffer::m_globalHardBuffersCount;

GlBuffer::GlBuffer()
        : m_bound(false)
        , m_finalVertexBufferSize(0)
        , m_finalIndexBufferSize(0)
        , m_finalBorderIndexBufferSize(0)
        , m_glHardBufferIds{
                  GL_BUFFER_UNKNOWN, GL_BUFFER_UNKNOWN, GL_BUFFER_UNKNOWN}
{
    m_vertices.reserve(MAX_VERTEX_BUFFER_SIZE);
    m_indices.reserve(MAX_INDEX_BUFFER_SIZE);
    m_borderIndices.reserve(MAX_INDEX_BUFFER_SIZE);
}

GlBuffer::GlBuffer(GlBuffer&& other)
{
    *this = std::move(other);
}

GlBuffer::~GlBuffer()
{
    m_globalVertexBufferSize.fetch_sub(getVertexBufferSize());
    m_globalIndexBufferSize.fetch_sub(getIndexBufferSize());
    m_globalBorderIndexBufferSize.fetch_sub(
            getIndexBufferSize(BF_BORDER_INDICES));

    if (m_bound) {
        glDeleteBuffers(GL_BUFFERS_COUNT, m_glHardBufferIds);
        m_globalHardBuffersCount.fetch_sub(GL_BUFFERS_COUNT);
    }
}

GlBuffer& GlBuffer::operator=(GlBuffer&& other)
{
    if (this == &other) {
        return *this;
    }

    m_globalVertexBufferSize.fetch_add(other.getVertexBufferSize());
    m_globalIndexBufferSize.fetch_add(other.getIndexBufferSize());
    m_globalBorderIndexBufferSize.fetch_add(
            other.getIndexBufferSize(BF_BORDER_INDICES));

    m_bound = other.m_bound;
    m_finalVertexBufferSize = other.m_finalVertexBufferSize;
    m_finalIndexBufferSize = other.m_finalIndexBufferSize;
    m_finalBorderIndexBufferSize = other.m_finalBorderIndexBufferSize;
    m_glHardBufferIds[0] = other.m_glHardBufferIds[0];
    m_glHardBufferIds[1] = other.m_glHardBufferIds[1];
    m_glHardBufferIds[2] = other.m_glHardBufferIds[2];

    other.m_bound = false;
    other.m_finalVertexBufferSize = 0;
    other.m_finalIndexBufferSize = 0;
    other.m_finalBorderIndexBufferSize = 0;
    other.m_glHardBufferIds[0] = GL_BUFFER_UNKNOWN;
    other.m_glHardBufferIds[1] = GL_BUFFER_UNKNOWN;
    other.m_glHardBufferIds[2] = GL_BUFFER_UNKNOWN;

    m_vertices.clear();
    m_vertices.shrink_to_fit();
    m_vertices = other.m_vertices;
    other.m_vertices.clear();
    other.m_vertices.shrink_to_fit();

    m_indices.clear();
    m_indices.shrink_to_fit();
    m_indices = other.m_indices;
    other.m_indices.clear();
    other.m_indices.shrink_to_fit();

    m_borderIndices.clear();
    m_borderIndices.shrink_to_fit();
    m_borderIndices = other.m_borderIndices;
    other.m_borderIndices.clear();
    other.m_borderIndices.shrink_to_fit();

    return *this;
}

void GlBuffer::bind()
{
    if (m_bound || m_vertices.empty() || m_indices.empty())
        return;

    m_globalHardBuffersCount.fetch_add(GL_BUFFERS_COUNT);
    ngsCheckGLEerror(glGenBuffers(GL_BUFFERS_COUNT, m_glHardBufferIds));

    m_finalVertexBufferSize = m_vertices.size();
    ngsCheckGLEerror(
            glBindBuffer(GL_ARRAY_BUFFER, getGlHardBufferId(BF_VERTICES)));
    ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER,
            sizeof(GLfloat) * m_finalVertexBufferSize, m_vertices.data(),
            GL_STATIC_DRAW));
    m_vertices.clear();

    m_finalIndexBufferSize = m_indices.size();
    ngsCheckGLEerror(glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER, getGlHardBufferId(BF_INDICES)));
    ngsCheckGLEerror(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            sizeof(GLushort) * m_finalIndexBufferSize, m_indices.data(),
            GL_STATIC_DRAW));
    m_indices.clear();

    if (!m_borderIndices.empty()) {
        m_finalBorderIndexBufferSize = m_borderIndices.size();
        ngsCheckGLEerror(glBindBuffer(
                GL_ELEMENT_ARRAY_BUFFER, getGlHardBufferId(BF_BORDER_INDICES)));
        ngsCheckGLEerror(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                sizeof(GLushort) * m_finalBorderIndexBufferSize,
                m_borderIndices.data(), GL_STATIC_DRAW));
        m_borderIndices.clear();
    }

    m_bound = true;
}

bool GlBuffer::bound() const
{
    return m_bound;
}

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

GLuint GlBuffer::getGlHardBufferId(ngsBufferType type) const
{
    switch (type) {
        case BF_VERTICES:
            return m_glHardBufferIds[0];
        case BF_INDICES:
            return m_glHardBufferIds[1];
        case BF_BORDER_INDICES:
            return m_glHardBufferIds[2];
    }
    return GL_BUFFER_UNKNOWN;
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
