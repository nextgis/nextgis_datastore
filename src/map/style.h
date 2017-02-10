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

#ifndef NGSSTYLE_H
#define NGSSTYLE_H

#include "glview.h"
#include "matrix.h"

namespace ngs
{
class Style
{
public:
    Style();
    virtual ~Style();
    virtual bool prepareProgram();
    virtual bool prepareData(const Matrix4& msMatrix, const Matrix4& vsMatrix);
    virtual void setColor(const ngsRGBA& color);
    virtual void draw(const GlBuffer& buffer) const;

protected:
    virtual const GLchar* getShaderSource(enum ngsShaderType type);

protected:
    const GLchar* m_vertexShaderSourcePtr;
    const GLchar* m_fragmentShaderSourcePtr;

    GlProgramUPtr m_program;
    bool m_load;

    GLint m_mPositionId;
    GLint m_msMatrixId;
    GLint m_vsMatrixId;
    GLint m_colorId;

    GlColor m_color;
};

using StyleUPtr = std::unique_ptr<Style>;

class SimplePointStyle : public Style
{
public:
    SimplePointStyle();
    virtual ~SimplePointStyle();

    float getRadius() const;
    void setRadius(float radius);

    // Style interface
public:
    virtual bool prepareData(
            const Matrix4& msMatrix, const Matrix4& vsMatrix) override;

    // Style interface
protected:
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    const GLchar* const m_pointVertexShaderSourcePtr = R"(
        attribute vec3 a_mPosition;

        uniform mat4 u_msMatrix;
        uniform float u_vRadius;

        void main()
        {
            gl_Position = u_msMatrix * vec4(a_mPosition, 1);
            gl_PointSize = u_vRadius;
        }
    )";

    // TODO: quad and triangle symbol. Sphere symbol (http://stackoverflow.com/a/25783231/2901140)
    // https://www.raywenderlich.com/37600/opengl-es-particle-system-tutorial-part-1
    // http://stackoverflow.com/a/10506172/2901140
    // https://www.cs.uaf.edu/2009/spring/cs480/lecture/02_03_pretty.html
    // http://stackoverflow.com/q/18659332/2901140
    const GLchar* const m_pointFragmentShaderSourcePtr = R"(
        precision mediump float;

        uniform vec4 u_color;

        void main()
        {
           vec2 coord = gl_PointCoord - vec2(0.5);
           if(length(coord) > 0.5) {
               discard;
           } else {
               gl_FragColor = u_color;
           }
        }
    )";

    GLint m_vRadiusId;

    float m_radius;
};

class SimpleLineStyle : public Style
{
public:
    SimpleLineStyle();
    virtual ~SimpleLineStyle();

    float getLineWidth() const;
    void setLineWidth(float lineWidth);

    // Style interface
public:
    virtual bool prepareData(
            const Matrix4& msMatrix, const Matrix4& vsMatrix) override;

    // Style interface
protected:
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    const GLchar* const m_lineVertexShaderSourcePtr = R"(
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

    const GLchar* const m_lineFragmentShaderSourcePtr = R"(
        precision mediump float;

        uniform vec4 u_color;

        void main()
        {
          gl_FragColor = u_color;
        }
    )";

    GLint m_normalId;
    GLint m_vLineWidthId;

    float m_lineWidth;
};

class SimpleFillStyle : public Style
{
public:
    SimpleFillStyle();
    virtual ~SimpleFillStyle();

    // Style interface
protected:
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    const GLchar* const m_fillVertexShaderSourcePtr = R"(
        attribute vec3 a_mPosition;

        uniform mat4 u_msMatrix;

        void main()
        {
            gl_Position = u_msMatrix * vec4(a_mPosition, 1);
        }
    )";

    const GLchar* const m_fillFragmentShaderSourcePtr = R"(
        precision mediump float;

        uniform vec4 u_color;

        void main()
        {
          gl_FragColor = u_color;
        }
    )";
};

class SimpleFillBorderedStyle : public Style
{
public:
    SimpleFillBorderedStyle();
    virtual ~SimpleFillBorderedStyle();

    float getBorderWidth() const;
    void setBorderWidth(float borderWidth);
    void setBorderColor(const ngsRGBA& color);
    void setBorderInicesCount(size_t count);

    // Style interface
public:
    virtual bool prepareData(
            const Matrix4& msMatrix, const Matrix4& vsMatrix) override;

    // Style interface
protected:
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    const GLchar* const m_fillBorderVertexShaderSourcePtr = R"(
        attribute vec3 a_mPosition;
        attribute vec2 a_normal;

        uniform bool u_isBorder;
        uniform float u_vBorderWidth;
        uniform mat4 u_msMatrix;
        uniform mat4 u_vsMatrix;

        void main()
        {
            if (u_isBorder) {
                vec4 vDelta = vec4(a_normal * u_vBorderWidth, 0, 0);
                vec4 sDelta = u_vsMatrix * vDelta;
                vec4 sPosition = u_msMatrix * vec4(a_mPosition, 1);
                gl_Position = sPosition + sDelta;
            } else {
                gl_Position = u_msMatrix * vec4(a_mPosition, 1);
            }
        }
    )";

    const GLchar* const m_fillBorderFragmentShaderSourcePtr = R"(
        precision mediump float;

        uniform bool u_isBorder;
        uniform vec4 u_color;
        uniform vec4 u_borderColor;

        void main()
        {
            if (u_isBorder) {
                gl_FragColor = u_borderColor;
            } else {
                gl_FragColor = u_color;
            }
        }
    )";

    GLint m_isBorderId;
    GLint m_normalId;
    GLint m_vBorderWidthId;
    GLint m_borderColorId;

    GlColor m_borederColor;
    float m_borderWidth;
};

class SimpleRasterStyle : public Style
{
public:
    SimpleRasterStyle();
    virtual ~SimpleRasterStyle();
};
}  // namespace ngs

#endif  // NGSSTYLE_H
