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
    virtual void prepare(const Matrix4 & mat);
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

}

#endif // STYLE_H
