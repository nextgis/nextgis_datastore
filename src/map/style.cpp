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

void Style::prepare(const Matrix4 &mat)
{
    if(!m_load) {
        m_load = m_program->load(getShaderSource(VERTEX),
                                 getShaderSource(FRAGMENT));
    }

    if(!m_load)
        return;

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


const GLchar *ngs::SimpleFillStyle::getShaderSource(ShaderType type)
{
    switch (type) {
    case VERTEX:
        return m_vertexShaderSourcePtr;
    case FRAGMENT:
        return m_fragmentShaderSourcePtr;
    }
}
