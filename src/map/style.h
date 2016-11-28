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

#ifndef STYLE_H
#define STYLE_H

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
    virtual void draw(const GlBuffer& buffer) const = 0;

protected:
    virtual const GLchar* getShaderSource(enum ngsShaderType type) = 0;

protected:
    GlProgramUPtr m_program;
    GLint m_mPositionId;
    GLint m_NormalId;
    GLint m_vLineWidthId;
    GLint m_msMatrixId;
    GLint m_vsMatrixId;
    GLint m_colorId;
    GlColor m_color;
    bool m_load;
};

using StyleUPtr = std::unique_ptr<Style>;

class SimpleFillStyle : public Style
{
public:
    SimpleFillStyle();
    virtual ~SimpleFillStyle();

    // Style interface
protected:
    virtual const GLchar* getShaderSource(enum ngsShaderType type) override;
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    const GLchar* const m_vertexShaderSourcePtr = R"(
        attribute vec3 a_mPosition;
        attribute vec2 a_Normal;

        uniform float u_vLineWidth;
        uniform mat4 u_msMatrix;
        uniform mat4 u_vsMatrix;

        void main()
        {
            vec4 vDelta = vec4(a_Normal * u_vLineWidth, 0, 0);
            vec4 sDelta = u_vsMatrix * vDelta;
            vec4 sPosition = u_msMatrix * vec4(a_mPosition, 1);
            gl_Position = sPosition + sDelta;
        }
    )";

    const GLchar* const m_fragmentShaderSourcePtr = R"(
        precision mediump float;
        uniform vec4 u_Color;
        void main()
        {
          gl_FragColor = u_Color;
        }
    )";
};

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
    virtual const GLchar* getShaderSource(enum ngsShaderType type) override;
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    const GLchar* const m_vertexShaderSourcePtr = R"(
        attribute vec4 vPosition;
        uniform mat4 mvMatrix;
        uniform float fRadius;
        void main()
        {
           gl_Position = mvMatrix * vPosition;
           gl_PointSize = fRadius;
        }
    )";

    // TODO: quad and triangle symbol. Sphere symbol (http://stackoverflow.com/a/25783231/2901140)
    // https://www.raywenderlich.com/37600/opengl-es-particle-system-tutorial-part-1
    // http://stackoverflow.com/a/10506172/2901140
    // https://www.cs.uaf.edu/2009/spring/cs480/lecture/02_03_pretty.html
    // http://stackoverflow.com/q/18659332/2901140
    const GLchar* const m_fragmentShaderSourcePtr = R"(
        precision mediump float;
        uniform vec4 u_Color;
        void main()
        {
           vec2 coord = gl_PointCoord - vec2(0.5);
           if(length(coord) > 0.5) {
               discard;
           } else {
               gl_FragColor = u_Color;
           }
        }
    )";

    float m_radius;
    GLint m_radiusId;
};

class SimpleRasterStyle : public Style
{
public:
    SimpleRasterStyle();
    virtual ~SimpleRasterStyle();
};
}    // namespace ngs

#endif    // STYLE_H
