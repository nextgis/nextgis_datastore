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
#ifndef NGSGLFUNCTIONS_H
#define NGSGLFUNCTIONS_H

#if __APPLE__
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE
        #define GLES
        #include <OpenGLES/ES2/gl.h>
        #include <OpenGLES/ES2/glext.h>
    #elif TARGET_IPHONE_SIMULATOR
        #include <OpenGLES/ES2/gl.h>
        #include <OpenGLES/ES2/glext.h>
    #elif TARGET_OS_MAC
        #include <OpenGL/OpenGL.h>
        #include <OpenGL/gl.h>
    #else
        #error Unsupported Apple platform
    #endif
#elif __ANDROID__
    #define GL_GLEXT_PROTOTYPES
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    #define GL_GLEXT_PROTOTYPES
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
//    #include <GL/gl.h>
//    #include <GL/glext.h>
#endif

#ifdef USE_EGL // need for headless desktop drawing (i.e. some preview generation)
#include "EGL/egl.h"
#endif // USE_EGL

namespace ngs {

#if defined(_DEBUG)
#define ngsCheckGLEerror(cmd) {cmd; checkGLError(#cmd);}
#define ngsCheckEGLEerror(cmd) {cmd; checkEGLError(#cmd);}
#else
#define ngsCheckGLEerror(cmd) (cmd)
#define ngsCheckEGLEerror(cmd) (cmd)
#endif

#ifdef USE_EGL
bool checkEGLError(const char *cmd);
#endif // USE_EGL

bool checkGLError(const char *cmd);
void reportGlStatus(GLuint obj);

}

#endif // NGSGLFUNCTIONS_H

/*

#define GL_BUFFERS_COUNT 3
#define VERTEX_SIZE 3
#define VERTEX_WITH_NORMAL_SIZE 5  // 5 = 3 for vertex + 2 for normal

class Style;


/*typedef int (*ngsBindVertexArray)(GLuint array);
typedef int (*ngsDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
typedef int (*ngsGenVertexArrays)(GLsizei n, GLuint* arrays);*//*

#define GL_BUFFER_UNKNOWN 0

enum ngsShaderType {
    SH_VERTEX,
    SH_FRAGMENT
};

enum ngsBufferType {
    BF_VERTICES = 0,
    BF_INDICES,
    BF_BORDER_INDICES
};

enum class LineCapType : uint8_t {
    Butt,
    Square,
    Round,
    // the following type is for internal use only
    FakeRound // TODO: remove it and switch to Round
};

enum class LineJoinType : uint8_t {
    Miter,
    Bevel,
    Round,
    // the following two types are for internal use only
    FlipBevel,
    FakeRound
};

class GlBuffer
{
public:
    explicit GlBuffer();
    explicit GlBuffer(const GlBuffer& other) = delete;
    explicit GlBuffer(GlBuffer&& otherr);
    ~GlBuffer();

    GlBuffer& operator=(const GlBuffer& other) = delete;
    GlBuffer& operator=(GlBuffer&& other);

    void bind();
    bool bound() const;

    static bool canGlobalStoreVertices(size_t amount, bool withNormals = false);
    static bool canGlobalStoreIndices(
            size_t amount, enum ngsBufferType indexType = BF_INDICES);

    bool canStoreVertices(size_t amount, bool withNormals = false) const;
    bool canStoreIndices(
            size_t amount, enum ngsBufferType indexType = BF_INDICES) const;

    void addVertex(float x,
            float y,
            float z,
            bool withNormals = false,
            float nX = 0,
            float nY = 0);
    void addIndex(
            unsigned short index, enum ngsBufferType indexType = BF_INDICES);
    void addTriangleIndices(unsigned short one,
            unsigned short two,
            unsigned short three,
            enum ngsBufferType indexType = BF_INDICES);

    size_t getVertexBufferSize() const;
    size_t getIndexBufferSize(enum ngsBufferType indexType = BF_INDICES) const;

    static std::int_fast32_t getGlobalVertexBufferSize();
    static std::int_fast32_t getGlobalIndexBufferSize(
            enum ngsBufferType indexType = BF_INDICES);
    static std::int_fast32_t getGlobalHardBuffersCount();

    GLuint getGlHardBufferId(enum ngsBufferType type) const;

protected:
    bool m_bound;
    size_t m_finalVertexBufferSize;
    size_t m_finalIndexBufferSize;
    size_t m_finalBorderIndexBufferSize;

    std::vector<GLfloat> m_vertices;
    std::vector<GLushort> m_indices;
    std::vector<GLushort> m_borderIndices;
    GLuint m_glHardBufferIds[GL_BUFFERS_COUNT];

    static std::atomic_int_fast32_t m_globalVertexBufferSize;
    static std::atomic_int_fast32_t m_globalIndexBufferSize;
    static std::atomic_int_fast32_t m_globalBorderIndexBufferSize;
    static std::atomic_int_fast32_t m_globalHardBuffersCount;
};

using GlBufferSharedPtr = std::shared_ptr<GlBuffer>;

// http://stackoverflow.com/a/13196986
template <typename... Args>
GlBufferSharedPtr makeSharedGlBuffer(Args&&... args)
{
    return std::make_shared<GlBuffer>(std::forward<Args>(args)...);
}

class GlBufferBucket
{
public:
    explicit GlBufferBucket(int x,
            int y,
            unsigned char z,
            const OGREnvelope& env,
            char crossExtent);
    explicit GlBufferBucket(const GlBufferBucket& bucket) = delete;
    explicit GlBufferBucket(GlBufferBucket&& bucket);
    ~GlBufferBucket();

    GlBufferBucket& operator=(const GlBufferBucket& bucket) = delete;
    GlBufferBucket& operator=(GlBufferBucket&& bucket);

    void bind();
    bool filled() const;
    void setFilled(bool filled);
    bool bound() const;
    void fill(GIntBig fid, OGRGeometry* geom, float level);
    void draw(const Style& style);
    int X() const;
    int Y() const;
    unsigned char zoom() const;
    void free();
    bool hasFid(GIntBig fid) const;
    int getFidCount() const;

    OGREnvelope extent() const;
    bool intersects(const GlBufferBucket& other) const;
    bool intersects(const OGREnvelope& ext) const;
    char crossExtent() const;

    size_t getVertexBufferSize() const;
    size_t getIndexBufferSize() const;

protected:
    void fill(const OGRGeometry* geom, float level);
    void fillPoint(const OGRPoint* point, float level);
    void fillLineString(const OGRLineString* line, float level);
    void fillPolygon(const OGRPolygon* polygon, float level);
    void fillBorderedPolygon(const OGRPolygon* polygon, float level);

    void addCurrentLineVertex(const Vector2& currPt,
            float level,
            const Vector2& normal,
            double endLeft,
            double endRight,
            bool round,
            GlBufferSharedPtr currBuffer);

    void addPieSliceLineVertex(const Vector2& currPt,
            float level,
            const Vector2& extrude,
            bool lineTurnsLeft,
            bool fakeRound,
            GlBufferSharedPtr currBuffer);

protected:
    std::vector<GlBufferSharedPtr> m_buffers;
    std::set<GIntBig> m_fids;
    int m_X;
    int m_Y;
    unsigned char m_zoom;
    OGREnvelope m_extent;
    bool m_filled;
    char m_crossExtent;
    int m_e1;
    int m_e2;
    int m_e3;
};

using GlBufferBucketSharedPtr = std::shared_ptr<GlBufferBucket>;

template <typename... Args>
GlBufferBucketSharedPtr makeSharedGlBufferBucket(Args&&... args)
{
    return std::make_shared<GlBufferBucket>(std::forward<Args>(args)...);
}

class GlFuctions
{
public:
    GlFuctions();
    ~GlFuctions();

    bool init();
    bool isOk() const;

    // Draw functions
    void testDraw(int colorId) const;
    void testDrawPreserved(int colorId) const;
    void drawPolygons(const std::vector<GLfloat> &vertices,
                      const std::vector<GLushort> &indices) const;
protected:
    bool loadExtensions();

protected:
    bool m_extensionLoad;
};
*/
