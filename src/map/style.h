/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
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

namespace ngs {

class Style
{
public:
    Style();
    virtual ~Style();
    virtual bool prepare(const Matrix4 & mat);
    virtual void setColor(const ngsRGBA &color);
    virtual void draw(const GlBuffer& buffer) const = 0;
protected:
    virtual const GLchar * getShaderSource(enum ngsShaderType type) = 0;
protected:
    GlProgramUPtr m_program;
    GLint m_matrixId, m_colorId;
    GlColor m_color;
    bool m_load;
};

typedef unique_ptr<Style> StyleUPtr;

class SimpleFillStyle : public Style
{
public:
    SimpleFillStyle();
    virtual ~SimpleFillStyle();

    // Style interface
protected:
    virtual const GLchar *getShaderSource(enum ngsShaderType type) override;
    virtual void draw(const GlBuffer &buffer) const override;

protected:
    const GLchar * const m_vertexShaderSourcePtr =
            "attribute vec4 vPosition;    \n"
            "uniform mat4 mvMatrix;       \n"
            "void main()                  \n"
            "{                            \n"
            "   gl_Position = mvMatrix * vPosition;  \n"
            "}                            \n";

    const GLchar * const m_fragmentShaderSourcePtr =
            "precision mediump float;     \n"
            "uniform vec4 u_Color;        \n"
            "void main()                  \n"
            "{                            \n"
            "  gl_FragColor = u_Color;    \n"
            "}                            \n";

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
    virtual bool prepare(const Matrix4 &mat) override;

    // Style interface
protected:
    virtual const GLchar *getShaderSource(enum ngsShaderType type) override;
    virtual void draw(const GlBuffer &buffer) const override;

protected:
    const GLchar * const m_vertexShaderSourcePtr =
            "attribute vec4 vPosition;    \n"
            "uniform mat4 mvMatrix;       \n"
            "uniform float fRadius;       \n"
            "void main()                  \n"
            "{                            \n"
            "   gl_Position = mvMatrix * vPosition;\n"
            "   gl_PointSize = fRadius;   \n"
            "}                            \n";

    // TODO: quad and triangle symbol. Sphere symbol (http://stackoverflow.com/a/25783231/2901140)
    // https://www.raywenderlich.com/37600/opengl-es-particle-system-tutorial-part-1
    // http://stackoverflow.com/a/10506172/2901140
    // https://www.cs.uaf.edu/2009/spring/cs480/lecture/02_03_pretty.html
    // http://stackoverflow.com/q/18659332/2901140
    const GLchar * const m_fragmentShaderSourcePtr =
            "precision mediump float;     \n"
            "uniform vec4 u_Color;        \n"
            "void main()                  \n"
            "{                            \n"
            "   vec2 coord = gl_PointCoord - vec2(0.5);\n"
            "   if(length(coord) > 0.5) { \n"
            "       discard;              \n"
            "   } else {                  \n"
            "       gl_FragColor = u_Color;\n"
            "   }                         \n"
            "}                            \n";
    float m_radius;
    GLint m_radiusId;

};

class SimpleRasterStyle : public Style
{
public:
    SimpleRasterStyle();
    virtual ~SimpleRasterStyle();
};

}

#endif // STYLE_H
