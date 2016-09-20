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
#include "style.h"

#include <iostream>
//#include "cpl_error.h"
//#include "cpl_string.h"

using namespace ngs;

//------------------------------------------------------------------------------
// Style
//------------------------------------------------------------------------------

Style::Style() : m_program(new GlProgram ()), m_matrixId(-1), m_colorId(-1),
    m_load(false)
{

}

Style::~Style()
{

}

bool Style::prepare(const Matrix4 &mat)
{
    if(!m_load) {
        m_load = m_program->load(getShaderSource(SH_VERTEX),
                                 getShaderSource(SH_FRAGMENT));
    }

    if(!m_load)
        return false;

    m_program->use();

#ifdef _DEBUG
    GLint numActiveUniforms = 0;
    glGetProgramiv(m_program->id(), GL_ACTIVE_UNIFORMS, &numActiveUniforms);
    cout << "Number active uniforms: " << numActiveUniforms << endl;
#endif //_DEBUG

    if(m_matrixId == -1)
        m_matrixId = glGetUniformLocation(m_program->id(), "mvMatrix");
    if(m_colorId == -1)
        m_colorId = glGetUniformLocation(m_program->id(), "u_Color");

    array<GLfloat, 16> mat4f = mat.dataF ();

    ngsCheckGLEerror(glUniformMatrix4fv(m_matrixId, 1, GL_FALSE, mat4f.data()));
    ngsCheckGLEerror(glUniform4f(m_colorId, m_color.r, m_color.g, m_color.b,
                                 m_color.a));

    return true;
}

void Style::setColor(const ngsRGBA &color)
{
    m_color.r = float(color.R) / 255;
    m_color.g = float(color.G) / 255;
    m_color.b = float(color.B) / 255;
    m_color.a = float(color.A) / 255;
}

//------------------------------------------------------------------------------
// SimpleFillStyle
//------------------------------------------------------------------------------


SimpleFillStyle::SimpleFillStyle() : Style()
{

}

SimpleFillStyle::~SimpleFillStyle()
{

}


const GLchar *SimpleFillStyle::getShaderSource(enum ngsShaderType type)
{
    switch (type) {
    case SH_VERTEX:
        return m_vertexShaderSourcePtr;
    case SH_FRAGMENT:
        return m_fragmentShaderSourcePtr;
    }
}


void SimpleFillStyle::draw(const GlBuffer &buffer) const
{
    if(!buffer.binded ())
        return;

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER,
                                  buffer.getBuffer (SH_VERTEX)));
    ngsCheckGLEerror(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                  buffer.getBuffer (SH_FRAGMENT)));
    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, buffer.getFinalIndicesCount (),
                                    GL_UNSIGNED_SHORT, NULL));
}

//------------------------------------------------------------------------------
// SimplePointStyle
//------------------------------------------------------------------------------


SimplePointStyle::SimplePointStyle() : Style(), m_radius(6), m_radiusId(-1)
{

}

SimplePointStyle::~SimplePointStyle()
{

}


const GLchar *SimplePointStyle::getShaderSource(enum ngsShaderType type)
{
    switch (type) {
    case SH_VERTEX:
        return m_vertexShaderSourcePtr;
    case SH_FRAGMENT:
        return m_fragmentShaderSourcePtr;
    }
}


void SimplePointStyle::draw(const GlBuffer &buffer) const
{
    if(!buffer.binded ())
        return;

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER,
                                  buffer.getBuffer (SH_VERTEX)));
    ngsCheckGLEerror(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                  buffer.getBuffer (SH_FRAGMENT)));
    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    //ngsCheckGLEerror(glDrawArrays(GL_POINTS, 0, buffer.getFinalIndicesCount ()));
    ngsCheckGLEerror(glDrawElements(GL_POINTS, buffer.getFinalIndicesCount (),
                                    GL_UNSIGNED_SHORT, NULL));
}

float SimplePointStyle::getRadius() const
{
    return m_radius;
}

void SimplePointStyle::setRadius(float radius)
{
    m_radius = radius;
}


bool SimplePointStyle::prepare(const Matrix4 &mat)
{
    if(!Style::prepare (mat))
        return false;
    if(m_radiusId == -1) {
        m_radiusId = glGetUniformLocation(m_program->id(), "fRadius");
    }
    ngsCheckGLEerror(glUniform1f(m_radiusId, m_radius));
    return true;
}
