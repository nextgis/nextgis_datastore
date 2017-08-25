/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 * Author: NikitaFeodonit, nfeodonit@yandex.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
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

#include "style.h"

#include <iostream>

#include "api_priv.h"

namespace ngs {

constexpr float normal45 = 0.70710678f;

float angle(const Normal &normal) {
    if(isEqual(normal.y, 0.0f)) {
        if(normal.x > 0.0f) {
            return 0.0f;
        }
        else {
            return M_PI_F;
        }
    }

    if(isEqual(normal.x, 0.0f)) {
        if(normal.y > 0.0f) {
            return M_PI_2_F;
        }
        else {
            return -M_PI_2_F;
        }
    }

    float angle = std::fabs(asinf(normal.y));
    if(normal.x < 0.0f && normal.y >= 0.0f)
        angle = M_PI_F - angle;
    else if(normal.x < 0.0f && normal.y <= 0.0f)
        angle = angle - M_PI_F;
    else if(normal.x > 0.0f && normal.y <= 0.0f)
        angle = -angle;
    return angle;
}

//------------------------------------------------------------------------------
// Style
//------------------------------------------------------------------------------

Style::Style()
        : m_vertexShaderSource(nullptr),
          m_fragmentShaderSource(nullptr)
{
}

const GLchar* Style::getShaderSource(ShaderType type)
{
    switch (type) {
        case SH_VERTEX:
            return m_vertexShaderSource;
        case SH_FRAGMENT:
            return m_fragmentShaderSource;
    }
    return nullptr;
}

bool Style::prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                    enum GlBuffer::BufferType /*type*/)
{
    if (!m_program.loaded()) {
        bool result = m_program.load(getShaderSource(Style::SH_VERTEX),
                                     getShaderSource(Style::SH_FRAGMENT));
        if(!result) {
            return false;
        }
    }

    m_program.use();

    m_program.setMatrix("u_msMatrix", msMatrix.dataF());
    m_program.setMatrix("u_vsMatrix", vsMatrix.dataF());

    return true;
}

void Style::draw(const GlBuffer& buffer) const
{
    if (!buffer.bound())
        return;

    buffer.rebind();
}

Style *Style::createStyle(const char *name)
{
    // NOTE: Add new styles here
    if(EQUAL(name, "simpleImage"))
        return new SimpleImageStyle;
    else if(EQUAL(name, "simplePoint"))
        return new SimplePointStyle;
    else if(EQUAL(name, "simpleLine"))
        return new SimpleLineStyle;
    else if(EQUAL(name, "simpleFill"))
        return new SimpleFillStyle;
    else if(EQUAL(name, "simpleFillBordered"))
        return new SimpleFillBorderedStyle;
    else if(EQUAL(name, "primitivePoint"))
        return new PrimitivePointStyle;
    return nullptr;
}


//------------------------------------------------------------------------------
// SimpleFillBorderStyle
//------------------------------------------------------------------------------

SimpleVectorStyle::SimpleVectorStyle() :
    Style(),
    m_color({1.0, 1.0, 1.0, 1.0})
{

}

bool SimpleVectorStyle::prepare(const Matrix4 &msMatrix, const Matrix4 &vsMatrix,
                                enum GlBuffer::BufferType type)
{
    if(!Style::prepare(msMatrix, vsMatrix, type))
        return false;
    m_program.setColor("u_color", m_color);

    return true;
}

bool SimpleVectorStyle::load(const CPLJSONObject &store)
{
    ngsRGBA color = ngsHEX2RGBA(store.GetString("color",
                                             ngsRGBA2HEX({255, 255, 255, 255})));
    setColor(color);
    return true;
}

CPLJSONObject SimpleVectorStyle::save() const
{
    CPLJSONObject out;
    out.Add("color", ngsRGBA2HEX(ngsGl2RGBA(m_color)));
    return out;
}

//------------------------------------------------------------------------------
// PointStyle
//------------------------------------------------------------------------------
PointStyle::PointStyle(enum PointType type)
        : SimpleVectorStyle(),
          m_type(type),
          m_size(6.0)
{
    m_styleType = ST_POINT;
}

bool PointStyle::load(const CPLJSONObject &store)
{
    if(!SimpleVectorStyle::load(store))
        return false;
    m_size = static_cast<float>(store.GetDouble("size", 6.0));
    m_type = static_cast<enum PointType>(store.GetInteger("type", 3));
    return true;
}

CPLJSONObject PointStyle::save() const
{
    CPLJSONObject out = SimpleVectorStyle::save();
    out.Add("size", static_cast<double>(m_size));
    out.Add("type", m_type);
    return out;
}

//------------------------------------------------------------------------------
// SimplePointStyle
//------------------------------------------------------------------------------

constexpr const GLchar* const pointVertexShaderSource = R"(
    attribute vec3 a_mPosition;

    uniform mat4 u_msMatrix;
    uniform float u_vSize;

    void main()
    {
        gl_Position = u_msMatrix * vec4(a_mPosition, 1);
        gl_PointSize = u_vSize;
    }
)";

// Circle: http://stackoverflow.com/a/17275113
// Sphere symbol (http://stackoverflow.com/a/25783231)
// https://www.raywenderlich.com/37600/opengl-es-particle-system-tutorial-part-1
// http://stackoverflow.com/a/10506172
// https://www.cs.uaf.edu/2009/spring/cs480/lecture/02_03_pretty.html
// http://stackoverflow.com/q/18659332
constexpr const GLchar* const pointFragmentShaderSource = R"(
    uniform vec4 u_color;
    uniform int u_type;

    bool isInTriangle(vec2 point, vec2 p1, vec2 p2, vec2 p3)
    {
      float a = (p1.x - point.x) * (p2.y - p1.y)
              - (p2.x - p1.x) * (p1.y - point.y);
      float b = (p2.x - point.x) * (p3.y - p2.y)
              - (p3.x - p2.x) * (p2.y - point.y);
      float c = (p3.x - point.x) * (p1.y - p3.y)
              - (p1.x - p3.x) * (p3.y - point.y);

      if ((a >= 0.0 && b >= 0.0 && c >= 0.0)
            || (a <= 0.0 && b <= 0.0 && c <= 0.0))
        return true;
      else
        return false;
    }

    void drawSquare()
    {
        gl_FragColor = u_color;
    }

    void drawRectangle()
    {
        if(0.4 < gl_PointCoord.x && gl_PointCoord.x > 0.6)
            discard;
        else
            gl_FragColor = u_color;
    }

    void drawCircle()
    {
        vec2 coord = gl_PointCoord - vec2(0.5);
        if(length(coord) > 0.5)
           discard;
        else
           gl_FragColor = u_color;
    }

    void drawTriangle()
    {
        if(!isInTriangle(vec2(gl_PointCoord),
                vec2(0.0, 0.066), vec2(1.0, 0.066), vec2(0.5, 0.933)))
           discard;
        else
           gl_FragColor = u_color;
    }

    void drawDiamond()
    {
        if(!(isInTriangle(vec2(gl_PointCoord),
                vec2(0.2, 0.5), vec2(0.8, 0.5), vec2(0.5, 0.0))
            || isInTriangle(vec2(gl_PointCoord),
                vec2(0.2, 0.5), vec2(0.8, 0.5), vec2(0.5, 1.0))))
           discard;
        else
           gl_FragColor = u_color;
    }

    void drawStar()
    {
        float d1 = 0.4;
        float d2 = 0.6;

        bool a1 = isInTriangle(vec2(gl_PointCoord),
                vec2(d1, d1), vec2(d2, d1), vec2(0.5, 0.0));
        bool a2 = isInTriangle(vec2(gl_PointCoord),
                vec2(d2, d1), vec2(d2, d2), vec2(1.0, 0.5));
        bool a3 = isInTriangle(vec2(gl_PointCoord),
                vec2(d1, d2), vec2(d2, d2), vec2(0.5, 1.0));
        bool a4 = isInTriangle(vec2(gl_PointCoord),
                vec2(d1, d1), vec2(d1, d2), vec2(0.0, 0.5));
        bool a5 = isInTriangle(vec2(gl_PointCoord),
                vec2(d1, d1), vec2(d2, d2), vec2(d2, d1));
        bool a6 = isInTriangle(vec2(gl_PointCoord),
                vec2(d1, d1), vec2(d2, d2), vec2(d1, d2));

        if(!(a1 || a2 || a3 || a4 || a5 || a6))
           discard;
        else
           gl_FragColor = u_color;
    }

    void main()
    {
        if(1 == u_type)      // Square
            drawSquare();
        else if(2 == u_type) // Rectangle
            drawRectangle();
        else if(3 == u_type) // Circle
            drawCircle();
        else if(4 == u_type) // Triangle
            drawTriangle();
        else if(5 == u_type) // Diamond
            drawDiamond();
        else if(6 == u_type) // Star
            drawStar();
    }
)";

SimplePointStyle::SimplePointStyle(enum PointType type)
        : PointStyle(type)
{
    m_vertexShaderSource = pointVertexShaderSource;
    m_fragmentShaderSource = pointFragmentShaderSource;
}

unsigned short SimplePointStyle::addPoint(const SimplePoint& pt,
                                          unsigned short index, GlBuffer* buffer)
{
    buffer->addVertex(pt.x);
    buffer->addVertex(pt.y);
    buffer->addVertex(0.0f);
    buffer->addIndex(index++);
    return index;
}

bool SimplePointStyle::prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                               enum GlBuffer::BufferType type)
{
    if (!SimpleVectorStyle::prepare(msMatrix, vsMatrix, type))
        return false;

    m_program.setInt("u_type", m_type);
    m_program.setFloat("u_vSize", m_size);
    m_program.setVertexAttribPointer("a_mPosition", 3, 0, 0);

    return true;
}

void SimplePointStyle::draw(const GlBuffer& buffer) const
{
    SimpleVectorStyle::draw(buffer);

    ngsCheckGLError(glDrawElements(GL_POINTS, buffer.indexSize(),
                                   GL_UNSIGNED_SHORT, nullptr));
}


//------------------------------------------------------------------------------
// SimpleLineStyle
//------------------------------------------------------------------------------
constexpr const GLchar* const lineVertexShaderSource = R"(
    attribute vec3 a_mPosition;
    attribute vec2 a_normal;

    uniform float u_vLineWidth;
    uniform mat4 u_msMatrix;
    uniform mat4 u_vsMatrix;

    void main()
    {
        vec4 vDelta = vec4(a_normal * u_vLineWidth, 0, 0);
        vec4 sDelta = u_vsMatrix * vDelta;
        vec4 sPosition = u_msMatrix * vec4(a_mPosition, 1);
        gl_Position = sPosition + sDelta;
    }
)";

constexpr const GLchar* const lineFragmentShaderSource = R"(
    uniform vec4 u_color;

    void main()
    {
      gl_FragColor = u_color;
    }
)";


SimpleLineStyle::SimpleLineStyle() : SimpleVectorStyle(),
          m_width(1.0),
          m_capType(CT_ROUND),
          m_joinType(JT_ROUND),
          m_segmentCount(6)
{
    m_vertexShaderSource = lineVertexShaderSource;
    m_fragmentShaderSource = lineFragmentShaderSource;
    m_styleType = ST_LINE;
}

bool SimpleLineStyle::prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                              enum GlBuffer::BufferType type)
{
    if (!SimpleVectorStyle::prepare(msMatrix, vsMatrix, type))
        return false;

    m_program.setFloat("u_vLineWidth", m_width);
    m_program.setVertexAttribPointer("a_mPosition", 3, 5 * sizeof(float), 0);
    m_program.setVertexAttribPointer("a_normal", 2, 5 * sizeof(float),
                            reinterpret_cast<const GLvoid*>(3 * sizeof(float)));
    return true;
}

void SimpleLineStyle::draw(const GlBuffer& buffer) const
{
    if(buffer.indexSize() == 0)
        return;
    SimpleVectorStyle::draw(buffer);
    ngsCheckGLError(glDrawElements(GL_TRIANGLES, buffer.indexSize(),
                                   GL_UNSIGNED_SHORT, nullptr));
}

bool SimpleLineStyle::load(const CPLJSONObject &store)
{
    if(!SimpleVectorStyle::load(store))
        return false;
    m_width = static_cast<float>(store.GetDouble("line_width", 3.0));
    m_capType = static_cast<enum CapType>(store.GetInteger("cap", m_capType));
    m_joinType = static_cast<enum JoinType>(store.GetInteger("join", m_joinType));
    m_segmentCount = static_cast<unsigned char>(store.GetInteger("segments", m_segmentCount));
    return true;
}

CPLJSONObject SimpleLineStyle::save() const
{
    CPLJSONObject out = SimpleVectorStyle::save();
    out.Add("line_width", static_cast<double>(m_width));
    out.Add("cap", m_capType);
    out.Add("join", m_joinType);
    out.Add("segments", m_segmentCount);
    return out;
}

enum CapType SimpleLineStyle::capType() const
{
    return m_capType;
}

void SimpleLineStyle::setCapType(const enum CapType &capType)
{
    m_capType = capType;
}

enum JoinType SimpleLineStyle::joinType() const
{
    return m_joinType;
}

void SimpleLineStyle::setJoinType(const enum JoinType &joinType)
{
    m_joinType = joinType;
}

unsigned char SimpleLineStyle::segmentCount() const
{
    return m_segmentCount;
}

void SimpleLineStyle::setSegmentCount(unsigned char segmentCount)
{
    m_segmentCount = segmentCount;
}

unsigned short SimpleLineStyle::addLineCap(const SimplePoint& point,
                                           const Normal& normal,
                                           unsigned short index, GlBuffer* buffer)
{
    switch(m_capType) {
        case CapType::CT_ROUND:
        {
            float start = asinf(normal.y);
            if(normal.x < 0.0f && normal.y <= 0.0f)
                start = M_PI_F + -(start);
            else if(normal.x < 0.0f && normal.y >= 0.0f)
                start = M_PI_2_F + start;
            else if(normal.x > 0.0f && normal.y <= 0.0f)
                start = M_PI_F + M_PI_F + start;

            float end = M_PI_F + start;
            float step = (end - start) / m_segmentCount;
            float current = start;
            for(int i = 0 ; i < m_segmentCount; ++i) {
                float x = cosf(current);
                float y = sinf(current);
                current += step;
                buffer->addVertex(point.x);
                buffer->addVertex(point.y);
                buffer->addVertex(0.0f);
                buffer->addVertex(x);
                buffer->addVertex(y);

                x = cosf(current);
                y = sinf(current);
                buffer->addVertex(point.x);
                buffer->addVertex(point.y);
                buffer->addVertex(0.0f);
                buffer->addVertex(x);
                buffer->addVertex(y);

                buffer->addVertex(point.x);
                buffer->addVertex(point.y);
                buffer->addVertex(0.0f);
                buffer->addVertex(0.0f);
                buffer->addVertex(0.0f);

                buffer->addIndex(index++);
                buffer->addIndex(index++);
                buffer->addIndex(index++);
            }
        }
            break;
        case CapType::CT_BUTT:
            break;
        case CapType::CT_SQUARE:
        {
        float scX1 = -(normal.y + normal.x);
        float scY1 = -(normal.y - normal.x);
        float scX2 = normal.x - normal.y;
        float scY2 = normal.x + normal.y;

        // 0
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(scX1);
        buffer->addVertex(scY1);

        // 1
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(scX2);
        buffer->addVertex(scY2);

        // 2
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(-normal.x);
        buffer->addVertex(-normal.y);

        // 3
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(normal.x);
        buffer->addVertex(normal.y);

        buffer->addIndex(index + 0);
        buffer->addIndex(index + 1);
        buffer->addIndex(index + 2);

        buffer->addIndex(index + 3);
        buffer->addIndex(index + 2);
        buffer->addIndex(index + 1);

        index += 4;
        }
    }

    return index;
}

size_t SimpleLineStyle::lineCapVerticesCount() const
{
    switch(m_capType) {
        case CapType::CT_ROUND:
            return 3 * m_segmentCount;
        case CapType::CT_BUTT:
            return 0;
        case CapType::CT_SQUARE:
            return 2;
    }

    return 0;
}

unsigned short SimpleLineStyle::addLineJoin(const SimplePoint& point,
                                            const Normal& prevNormal,
                                            const Normal& normal,
                                            unsigned short index,
                                            GlBuffer* buffer)
{
//    float maxWidth = width() * 5;
    float start = angle(prevNormal);
    float end = angle(normal);

    float angle = end - start;
    char mult = angle >= 0 ? -1 : 1;

    switch(m_joinType) {
    case JoinType::JT_ROUND:
    {
        float step = angle / m_segmentCount;
        float current = start;
        for(int i = 0 ; i < m_segmentCount; ++i) {
            float x = cosf(current) * mult;
            float y = sinf(current) * mult;

            buffer->addVertex(point.x);
            buffer->addVertex(point.y);
            buffer->addVertex(0.0f);
            buffer->addVertex(x);
            buffer->addVertex(y);

            current += step;
            x = cosf(current) * mult;
            y = sinf(current) * mult;
            buffer->addVertex(point.x);
            buffer->addVertex(point.y);
            buffer->addVertex(0.0f);
            buffer->addVertex(x);
            buffer->addVertex(y);

            buffer->addVertex(point.x);
            buffer->addVertex(point.y);
            buffer->addVertex(0.0f);
            buffer->addVertex(0.0f);
            buffer->addVertex(0.0f);

            buffer->addIndex(index++);
            buffer->addIndex(index++);
            buffer->addIndex(index++);
        }
    }
        break;
    case JoinType::JT_MITER:
    {
        Normal newNormal;
        newNormal.x = (prevNormal.x + normal.x);
        newNormal.y = (prevNormal.y + normal.y);
        float cosHalfAngle = newNormal.x * normal.x + newNormal.y * normal.y;
        float miterLength = cosHalfAngle == 0.0f ? 0.0f : 1.0f / cosHalfAngle;
        newNormal.x *= miterLength;
        newNormal.y *= miterLength;

        // 0
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(prevNormal.x * mult);
        buffer->addVertex(prevNormal.y * mult);

        // 1
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(newNormal.x * mult);
        buffer->addVertex(newNormal.y * mult);

        // 2
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.0f);

        buffer->addIndex(index++);
        buffer->addIndex(index++);
        buffer->addIndex(index++);

        // 0
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(normal.x * mult);
        buffer->addVertex(normal.y * mult);

        // 1
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(newNormal.x * mult);
        buffer->addVertex(newNormal.y * mult);

        // 2
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.0f);

        buffer->addIndex(index++);
        buffer->addIndex(index++);
        buffer->addIndex(index++);
    }
        break;
    case JoinType::JT_BEVELED:
    {
        // 0
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(prevNormal.x * mult);
        buffer->addVertex(prevNormal.y * mult);

        // 1
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(normal.x * mult);
        buffer->addVertex(normal.y * mult);

        // 2
        buffer->addVertex(point.x);
        buffer->addVertex(point.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.0f);

        buffer->addIndex(index++);
        buffer->addIndex(index++);
        buffer->addIndex(index++);
    }
    }
    return index;
}

size_t SimpleLineStyle::lineJoinVerticesCount() const
{
    switch(m_joinType) {
    case JoinType::JT_ROUND:
        return 3 * m_segmentCount;
    case JoinType::JT_MITER:
        return 6;
    case JoinType::JT_BEVELED:
        return 3;
    }

    return 0;
}

unsigned short SimpleLineStyle::addSegment(const SimplePoint& pt1,
                                           const SimplePoint& pt2,
                                           const Normal& normal,
                                           unsigned short index,
                                           GlBuffer* buffer)
{
    // 0
    buffer->addVertex(pt1.x);
    buffer->addVertex(pt1.y);
    buffer->addVertex(0.0f);
    buffer->addVertex(-normal.x);
    buffer->addVertex(-normal.y);
    buffer->addIndex(index++); // 0

    // 1
    buffer->addVertex(pt2.x);
    buffer->addVertex(pt2.y);
    buffer->addVertex(0.0f);
    buffer->addVertex(-normal.x);
    buffer->addVertex(-normal.y);
    buffer->addIndex(index++); // 1

    // 2
    buffer->addVertex(pt1.x);
    buffer->addVertex(pt1.y);
    buffer->addVertex(0.0f);
    buffer->addVertex(normal.x);
    buffer->addVertex(normal.y);
    buffer->addIndex(index++); // 2

    // 3
    buffer->addVertex(pt2.x);
    buffer->addVertex(pt2.y);
    buffer->addVertex(0.0f);
    buffer->addVertex(normal.x);
    buffer->addVertex(normal.y);

    buffer->addIndex(index - 2); // index = 3 at that point
    buffer->addIndex(index - 1);
    buffer->addIndex(index++);

    return index;
}


//------------------------------------------------------------------------------
// PrimitivePointStyle
//------------------------------------------------------------------------------
PrimitivePointStyle::PrimitivePointStyle(enum PointType type) : PointStyle(type),
    m_segmentCount(10)
{
    m_vertexShaderSource = lineVertexShaderSource;
    m_fragmentShaderSource = lineFragmentShaderSource;
    m_styleType = ST_POINT;
}

unsigned short PrimitivePointStyle::addPoint(const SimplePoint& pt,
                                             unsigned short index,
                                             GlBuffer* buffer)
{
    switch(pointType()) {
    case PT_SQUARE:
        {
        // 0
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(-normal45);
        buffer->addVertex(normal45);

        // 1
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(normal45);
        buffer->addVertex(normal45);

        // 2
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(normal45);
        buffer->addVertex(-normal45);

        // 3
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(-normal45);
        buffer->addVertex(-normal45);

        buffer->addIndex(index + 0);
        buffer->addIndex(index + 1);
        buffer->addIndex(index + 2);

        buffer->addIndex(index + 0);
        buffer->addIndex(index + 2);
        buffer->addIndex(index + 3);

        index += 4;
        }
        break;
    case PT_RECTANGLE:
        {
        // 0
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(-0.86602540f);
        buffer->addVertex(0.5f);

        // 1
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.86602540f);
        buffer->addVertex(0.5f);

        // 2
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.86602540f);
        buffer->addVertex(-0.5f);

        // 3
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(-0.86602540f);
        buffer->addVertex(-0.5f);

        buffer->addIndex(index + 0);
        buffer->addIndex(index + 1);
        buffer->addIndex(index + 2);

        buffer->addIndex(index + 0);
        buffer->addIndex(index + 2);
        buffer->addIndex(index + 3);

        index += 4;
        }
        break;
    case PT_CIRCLE:
        {
            float start = 0.0f;
            float end = M_PI_F + M_PI_F;
            float step = (end - start) / m_segmentCount;
            float current = start;
            for(int i = 0 ; i < m_segmentCount; ++i) {
                float x = cosf(current);
                float y = sinf(current);
                current += step;
                buffer->addVertex(pt.x);
                buffer->addVertex(pt.y);
                buffer->addVertex(0.0f);
                buffer->addVertex(x);
                buffer->addVertex(y);

                x = cosf(current);
                y = sinf(current);
                buffer->addVertex(pt.x);
                buffer->addVertex(pt.y);
                buffer->addVertex(0.0f);
                buffer->addVertex(x);
                buffer->addVertex(y);

                buffer->addVertex(pt.x);
                buffer->addVertex(pt.y);
                buffer->addVertex(0.0f);
                buffer->addVertex(0.0f);
                buffer->addVertex(0.0f);

                buffer->addIndex(index++);
                buffer->addIndex(index++);
                buffer->addIndex(index++);
            }
        }
        break;
    case PT_TRIANGLE:
        {
        // 0
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.0f);
        buffer->addVertex(1.0f);

        // 1
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(0.86602540f);
        buffer->addVertex(-0.5f);

        // 2
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(-0.86602540f);
        buffer->addVertex(-0.5f);

        buffer->addIndex(index + 0);
        buffer->addIndex(index + 1);
        buffer->addIndex(index + 2);

        index += 3;
        }
        break;
    case PT_DIAMOND:
        {
        // 0
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(1.0f);
        buffer->addVertex(0.0f);

        // 1
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(normal45);
        buffer->addVertex(0.0f);

        // 2
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(-normal45);
        buffer->addVertex(0.0f);

        // 3
        buffer->addVertex(pt.x);
        buffer->addVertex(pt.y);
        buffer->addVertex(0.0f);
        buffer->addVertex(-1.0f);
        buffer->addVertex(0.0f);

        buffer->addIndex(index + 0);
        buffer->addIndex(index + 1);
        buffer->addIndex(index + 2);

        buffer->addIndex(index + 1);
        buffer->addIndex(index + 2);
        buffer->addIndex(index + 3);

        index += 4;
        }
        break;
    case PT_STAR:
        // TODO: Star drawing
        break;
    default:
        break;
    }
    return index;
}

size_t PrimitivePointStyle::pointVerticesCount() const
{
    switch(pointType()) {
    case PT_SQUARE:
    case PT_RECTANGLE:
        return 4;
    case PT_CIRCLE:
        return m_segmentCount * 3;
    case PT_TRIANGLE:
        return 3;
    case PT_DIAMOND:
        return 4;
    case PT_STAR:
        // TODO: Star drawing
    default:
        return 0;
    }
}

bool PrimitivePointStyle::prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                              enum GlBuffer::BufferType type)
{
    if (!SimpleVectorStyle::prepare(msMatrix, vsMatrix, type))
        return false;

    m_program.setFloat("u_vLineWidth", m_size);
    m_program.setVertexAttribPointer("a_mPosition", 3, 5 * sizeof(float), 0);
    m_program.setVertexAttribPointer("a_normal", 2, 5 * sizeof(float),
                            reinterpret_cast<const GLvoid*>(3 * sizeof(float)));
    return true;
}

void PrimitivePointStyle::draw(const GlBuffer& buffer) const
{
    if(buffer.indexSize() == 0)
        return;
    SimpleVectorStyle::draw(buffer);
    ngsCheckGLError(glDrawElements(GL_TRIANGLES, buffer.indexSize(),
                                   GL_UNSIGNED_SHORT, nullptr));
}

bool PrimitivePointStyle::load(const CPLJSONObject &store)
{
    if(!PointStyle::load(store))
        return false;
    m_segmentCount = static_cast<unsigned char>(store.GetInteger("segments", m_segmentCount));
    return true;
}

CPLJSONObject PrimitivePointStyle::save() const
{
    CPLJSONObject out = PointStyle::save();
    out.Add("segments", m_segmentCount);
    return out;
}

//------------------------------------------------------------------------------
// SimpleFillStyle
//------------------------------------------------------------------------------
constexpr const GLchar* const fillVertexShaderSource = R"(
    attribute vec3 a_mPosition;

    uniform mat4 u_msMatrix;

    void main()
    {
        gl_Position = u_msMatrix * vec4(a_mPosition, 1);
    }
)";

constexpr const GLchar* const fillFragmentShaderSource = R"(
    uniform vec4 u_color;

    void main()
    {
      gl_FragColor = u_color;
    }
)";

SimpleFillStyle::SimpleFillStyle()
        : SimpleVectorStyle()
{
    m_vertexShaderSource = fillVertexShaderSource;
    m_fragmentShaderSource = fillFragmentShaderSource;
    m_styleType = ST_FILL;
}

bool SimpleFillStyle::prepare(const Matrix4 &msMatrix, const Matrix4 &vsMatrix,
                              enum GlBuffer::BufferType type)
{
    if (!SimpleVectorStyle::prepare(msMatrix, vsMatrix, type))
        return false;
    m_program.setVertexAttribPointer("a_mPosition", 3, 0, 0);

    return true;
}

void SimpleFillStyle::draw(const GlBuffer& buffer) const
{
    SimpleVectorStyle::draw(buffer);
    ngsCheckGLError(glDrawElements(GL_TRIANGLES, buffer.indexSize(),
            GL_UNSIGNED_SHORT, nullptr));
}

//------------------------------------------------------------------------------
// SimpleFillBorderStyle
//------------------------------------------------------------------------------

SimpleFillBorderedStyle::SimpleFillBorderedStyle()
        : Style()
{
    m_styleType = ST_FILL;
}

bool SimpleFillBorderedStyle::prepare(const Matrix4& msMatrix,
                                      const Matrix4& vsMatrix,
                                      enum GlBuffer::BufferType type)
{
    if(type == GlBuffer::BF_LINE) {
        if(!m_line.prepare(msMatrix, vsMatrix, type))
            return false;
    }
    else if(type == GlBuffer::BF_FILL) {
        if(!m_fill.prepare(msMatrix, vsMatrix, type))
            return false;
    }
    return true;
}

void SimpleFillBorderedStyle::draw(const GlBuffer& buffer) const
{
    if(buffer.type() == GlBuffer::BF_LINE) {
        m_line.draw(buffer);
    }
    else if(buffer.type() == GlBuffer::BF_FILL) {
        m_fill.draw(buffer);
    }
}

bool SimpleFillBorderedStyle::load(const CPLJSONObject &store)
{
    if(!m_line.load(store.GetObject("line")))
        return false;
    if(!m_fill.load(store.GetObject("fill")))
        return false;

    return true;
}

CPLJSONObject SimpleFillBorderedStyle::save() const
{
    CPLJSONObject out;
    out.Add("line", m_line.save());
    out.Add("fill", m_fill.save());
    return out;
}

unsigned char SimpleFillBorderedStyle::segmentCount() const
{
    return m_line.segmentCount();
}

void SimpleFillBorderedStyle::setSegmentCount(unsigned char segmentCount)
{
    m_line.setSegmentCount(segmentCount);
}

enum CapType SimpleFillBorderedStyle::capType() const
{
    return m_line.capType();
}

void SimpleFillBorderedStyle::setCapType(const enum CapType &capType)
{
    m_line.setCapType(capType);
}

enum JoinType SimpleFillBorderedStyle::joinType() const
{
    return m_line.joinType();
}

void SimpleFillBorderedStyle::setJoinType(const enum JoinType &joinType)
{
    m_line.setJoinType(joinType);
}

//------------------------------------------------------------------------------
// SimpleImageStyle
//------------------------------------------------------------------------------
constexpr const GLchar* const imageVertexShaderSource = R"(
    attribute vec3 a_mPosition;
    attribute vec2 a_texCoord;

    uniform mat4 u_msMatrix;
    varying vec2 v_texCoord;

    void main()
    {
        gl_Position = u_msMatrix * vec4(a_mPosition, 1);
        v_texCoord = a_texCoord;
    }
)";

constexpr const GLchar* const imageFragmentShaderSource = R"(
    varying vec2 v_texCoord;
    uniform sampler2D s_texture;

    void main()
    {
        gl_FragColor = texture2D( s_texture, v_texCoord );
    }
)";

SimpleImageStyle::SimpleImageStyle() : Style(), m_image(nullptr)
{
    m_vertexShaderSource = imageVertexShaderSource;
    m_fragmentShaderSource = imageFragmentShaderSource;
    m_styleType = ST_IMAGE;
}

bool SimpleImageStyle::prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                               enum GlBuffer::BufferType type)
{
    if (!Style::prepare(msMatrix, vsMatrix, type))
        return false;

    if(m_image) {
        if(m_image->bound()) {
            m_image->rebind();
        }
        else {
            m_image->bind();
        }
    }
    m_program.setInt("s_texture", 0);
    m_program.setVertexAttribPointer("a_mPosition", 3, 5 * sizeof(float), 0);
    m_program.setVertexAttribPointer("a_texCoord", 2, 5 * sizeof(float),
                            reinterpret_cast<const GLvoid*>(3 * sizeof(float)));

    return true;
}


void SimpleImageStyle::draw(const GlBuffer& buffer) const
{
    if(!m_image || !m_image->bound())
        return;

    Style::draw(buffer);

    ngsCheckGLError(glActiveTexture(GL_TEXTURE0));
    m_image->rebind();

    ngsCheckGLError(glDrawElements(GL_TRIANGLES, buffer.indexSize(),
            GL_UNSIGNED_SHORT, nullptr));
}

} // namespace ngs
