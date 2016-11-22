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
//#include "cpl_error.h"
//#include "cpl_string.h"

using namespace ngs;

//------------------------------------------------------------------------------
// Style
//------------------------------------------------------------------------------

Style::Style()
        : m_program(new GlProgram())
        , m_mPositionId(-1)
        , m_NormalId(-1)
        , m_vLineWidthId(-1)
        , m_msMatrixId(-1)
        , m_vsMatrixId(-1)
        , m_colorId(-1)
        , m_load(false)
{
}

Style::~Style()
{

}

bool Style::prepareProgram()
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

    return true;
}

bool Style::prepareData(const Matrix4 &msMatrix, const Matrix4 &vsMatrix)
{
    if(!m_load)
        return false;

    if(m_mPositionId == -1)
        m_mPositionId = glGetAttribLocation(m_program->id(), "a_mPosition");
    if(m_NormalId == -1)
        m_NormalId = glGetAttribLocation(m_program->id(), "a_Normal");

    if(m_vLineWidthId == -1)
        m_vLineWidthId = glGetUniformLocation(m_program->id(), "u_vLineWidth");
    if(m_msMatrixId == -1)
        m_msMatrixId = glGetUniformLocation(m_program->id(), "u_msMatrix");
    if(m_vsMatrixId == -1)
        m_vsMatrixId = glGetUniformLocation(m_program->id(), "u_vsMatrix");

    if(m_colorId == -1)
        m_colorId = glGetUniformLocation(m_program->id(), "u_Color");

    const float lineWidth = 2.0;
    ngsCheckGLEerror(glUniform1f(m_vLineWidthId, lineWidth));

    array<GLfloat, 16> msMat4f = msMatrix.dataF ();
    ngsCheckGLEerror(glUniformMatrix4fv(m_msMatrixId, 1, GL_FALSE, msMat4f.data()));

    array<GLfloat, 16> vsMat4f = vsMatrix.dataF ();
    ngsCheckGLEerror(glUniformMatrix4fv(m_vsMatrixId, 1, GL_FALSE, vsMat4f.data()));

    ngsCheckGLEerror(glUniform4f(m_colorId, m_color.r, m_color.g, m_color.b, m_color.a));

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
    if(!buffer.bound())
        return;

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffer.getBuffer(SH_VERTEX)));
    ngsCheckGLEerror(glEnableVertexAttribArray(m_mPositionId));
    ngsCheckGLEerror(
            glVertexAttribPointer(m_mPositionId, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0));

    ngsCheckGLEerror(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.getBuffer(SH_FRAGMENT)));
    ngsCheckGLEerror(glEnableVertexAttribArray(m_NormalId));
    ngsCheckGLEerror(glVertexAttribPointer(
            m_NormalId, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
            reinterpret_cast<const GLvoid*>(3 * sizeof(float))));

    ngsCheckGLEerror(glDrawElements(
            GL_TRIANGLES, buffer.getFinalIndexBufferSize(), GL_UNSIGNED_SHORT, NULL));
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
    if(!buffer.bound())
        return;

    ngsCheckGLEerror(
            glBindBuffer(GL_ARRAY_BUFFER, buffer.getBuffer(SH_VERTEX)));
    ngsCheckGLEerror(glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER, buffer.getBuffer(SH_FRAGMENT)));

    ngsCheckGLEerror(glEnableVertexAttribArray(m_mPositionId));
    ngsCheckGLEerror(
            glVertexAttribPointer(m_mPositionId, 3, GL_FLOAT, GL_FALSE, 0, 0));

    ngsCheckGLEerror(glDrawElements(GL_POINTS,
            buffer.getFinalIndexBufferSize(), GL_UNSIGNED_SHORT, NULL));
}

float SimplePointStyle::getRadius() const
{
    return m_radius;
}

void SimplePointStyle::setRadius(float radius)
{
    m_radius = radius;
}


bool SimplePointStyle::prepareData(const Matrix4 &msMatrix, const Matrix4 &vsMatrix)
{
    if (!Style::prepareData(msMatrix, vsMatrix))
        return false;
    if(m_radiusId == -1) {
        m_radiusId = glGetUniformLocation(m_program->id(), "fRadius");
    }
    ngsCheckGLEerror(glUniform1f(m_radiusId, m_radius));
    return true;
}
