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
        : m_vertexShaderSourcePtr(nullptr)
        , m_fragmentShaderSourcePtr(nullptr)
        , m_program(new GlProgram())
        , m_load(false)
        , m_mPositionId(-1)
        , m_msMatrixId(-1)
        , m_vsMatrixId(-1)
        , m_colorId(-1)
{
}

Style::~Style()
{
}

const GLchar* Style::getShaderSource(enum ngsShaderType type)
{
    switch (type) {
        case SH_VERTEX:
            return m_vertexShaderSourcePtr;
        case SH_FRAGMENT:
            return m_fragmentShaderSourcePtr;
    }
    return nullptr;
}

void Style::setColor(const ngsRGBA& color)
{
    m_color.r = float(color.R) / 255;
    m_color.g = float(color.G) / 255;
    m_color.b = float(color.B) / 255;
    m_color.a = float(color.A) / 255;
}

bool Style::prepareProgram()
{
    if (!m_load) {
        m_load = m_program->load(
                getShaderSource(SH_VERTEX), getShaderSource(SH_FRAGMENT));
    }

    if (!m_load)
        return false;

    m_program->use();

#ifdef _DEBUG
    GLint numActiveUniforms = 0;
    glGetProgramiv(m_program->id(), GL_ACTIVE_UNIFORMS, &numActiveUniforms);
    std::cout << "Number active uniforms: " << numActiveUniforms << "\n";
#endif  //_DEBUG

    return true;
}

bool Style::prepareData(const Matrix4& msMatrix, const Matrix4& vsMatrix)
{
    if (!m_load)
        return false;

    if (m_mPositionId == -1)
        m_mPositionId = glGetAttribLocation(m_program->id(), "a_mPosition");

    if (m_msMatrixId == -1)
        m_msMatrixId = glGetUniformLocation(m_program->id(), "u_msMatrix");
    if (m_vsMatrixId == -1)
        m_vsMatrixId = glGetUniformLocation(m_program->id(), "u_vsMatrix");

    if (m_colorId == -1)
        m_colorId = glGetUniformLocation(m_program->id(), "u_color");

    std::array<GLfloat, 16> msMat4f = msMatrix.dataF();
    ngsCheckGLEerror(
            glUniformMatrix4fv(m_msMatrixId, 1, GL_FALSE, msMat4f.data()));

    std::array<GLfloat, 16> vsMat4f = vsMatrix.dataF();
    ngsCheckGLEerror(
            glUniformMatrix4fv(m_vsMatrixId, 1, GL_FALSE, vsMat4f.data()));

    ngsCheckGLEerror(
            glUniform4f(m_colorId, m_color.r, m_color.g, m_color.b, m_color.a));

    return true;
}

void Style::draw(const GlBuffer& buffer) const
{
    if (!buffer.bound())
        return;

    ngsCheckGLEerror(glBindBuffer(
            GL_ARRAY_BUFFER, buffer.getGlHardBufferId(BF_VERTICES)));
    ngsCheckGLEerror(glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER, buffer.getGlHardBufferId(BF_INDICES)));
}

//------------------------------------------------------------------------------
// SimplePointStyle
//------------------------------------------------------------------------------

SimplePointStyle::SimplePointStyle()
        : Style()
        , m_vRadiusId(-1)
        , m_radius(6)
{
    m_vertexShaderSourcePtr = m_pointVertexShaderSourcePtr;
    m_fragmentShaderSourcePtr = m_pointFragmentShaderSourcePtr;
}

SimplePointStyle::~SimplePointStyle()
{
}

float SimplePointStyle::getRadius() const
{
    return m_radius;
}

void SimplePointStyle::setRadius(float radius)
{
    m_radius = radius;
}

bool SimplePointStyle::prepareData(
        const Matrix4& msMatrix, const Matrix4& vsMatrix)
{
    if (!Style::prepareData(msMatrix, vsMatrix))
        return false;
    if (m_vRadiusId == -1) {
        m_vRadiusId = glGetUniformLocation(m_program->id(), "u_vRadius");
    }
    ngsCheckGLEerror(glUniform1f(m_vRadiusId, m_radius));
    return true;
}

void SimplePointStyle::draw(const GlBuffer& buffer) const
{
    Style::draw(buffer);

    ngsCheckGLEerror(glEnableVertexAttribArray(m_mPositionId));
    ngsCheckGLEerror(
            glVertexAttribPointer(m_mPositionId, 3, GL_FLOAT, GL_FALSE, 0, 0));

    ngsCheckGLEerror(glDrawElements(GL_POINTS, buffer.getIndexBufferSize(),
            GL_UNSIGNED_SHORT, nullptr));
}

//------------------------------------------------------------------------------
// SimpleLineStyle
//------------------------------------------------------------------------------

SimpleLineStyle::SimpleLineStyle()
        : Style()
        , m_normalId(-1)
        , m_vLineWidthId(-1)
        , m_lineWidth(1)
{
    m_vertexShaderSourcePtr = m_lineVertexShaderSourcePtr;
    m_fragmentShaderSourcePtr = m_lineFragmentShaderSourcePtr;
}

SimpleLineStyle::~SimpleLineStyle()
{
}

float SimpleLineStyle::getLineWidth() const
{
    return m_lineWidth;
}

void SimpleLineStyle::setLineWidth(float lineWidth)
{
    m_lineWidth = lineWidth;
}

bool SimpleLineStyle::prepareData(
        const Matrix4& msMatrix, const Matrix4& vsMatrix)
{
    if (!Style::prepareData(msMatrix, vsMatrix))
        return false;

    if (m_normalId == -1)
        m_normalId = glGetAttribLocation(m_program->id(), "a_normal");

    if (m_vLineWidthId == -1)
        m_vLineWidthId = glGetUniformLocation(m_program->id(), "u_vLineWidth");
    ngsCheckGLEerror(glUniform1f(m_vLineWidthId, m_lineWidth));

    return true;
}

void SimpleLineStyle::draw(const GlBuffer& buffer) const
{
    Style::draw(buffer);

    ngsCheckGLEerror(glEnableVertexAttribArray(m_mPositionId));
    ngsCheckGLEerror(glVertexAttribPointer(
            m_mPositionId, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0));

    ngsCheckGLEerror(glEnableVertexAttribArray(m_normalId));
    ngsCheckGLEerror(glVertexAttribPointer(m_normalId, 2, GL_FLOAT, GL_FALSE,
            5 * sizeof(float),
            reinterpret_cast<const GLvoid*>(3 * sizeof(float))));

    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, buffer.getIndexBufferSize(),
            GL_UNSIGNED_SHORT, nullptr));
}

//------------------------------------------------------------------------------
// SimpleFillStyle
//------------------------------------------------------------------------------

SimpleFillStyle::SimpleFillStyle()
        : Style()
{
    m_vertexShaderSourcePtr = m_fillVertexShaderSourcePtr;
    m_fragmentShaderSourcePtr = m_fillFragmentShaderSourcePtr;
}

SimpleFillStyle::~SimpleFillStyle()
{
}

void SimpleFillStyle::draw(const GlBuffer& buffer) const
{
    Style::draw(buffer);

    ngsCheckGLEerror(glEnableVertexAttribArray(m_mPositionId));
    ngsCheckGLEerror(
            glVertexAttribPointer(m_mPositionId, 3, GL_FLOAT, GL_FALSE, 0, 0));

    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, buffer.getIndexBufferSize(),
            GL_UNSIGNED_SHORT, nullptr));
}

//------------------------------------------------------------------------------
// SimpleFillBorderStyle
//------------------------------------------------------------------------------

SimpleFillBorderedStyle::SimpleFillBorderedStyle()
        : Style()
        , m_isBorderId(-1)
        , m_normalId(-1)
        , m_vBorderWidthId(-1)
        , m_borderColorId(-1)
        , m_borderWidth(1)
{
    m_vertexShaderSourcePtr = m_fillBorderVertexShaderSourcePtr;
    m_fragmentShaderSourcePtr = m_fillBorderFragmentShaderSourcePtr;
}

SimpleFillBorderedStyle::~SimpleFillBorderedStyle()
{
}

float SimpleFillBorderedStyle::getBorderWidth() const
{
    return m_borderWidth;
}

void SimpleFillBorderedStyle::setBorderWidth(float borderWidth)
{
    m_borderWidth = borderWidth;
}

void SimpleFillBorderedStyle::setBorderColor(const ngsRGBA& color)
{
    m_borederColor.r = float(color.R) / 255;
    m_borederColor.g = float(color.G) / 255;
    m_borederColor.b = float(color.B) / 255;
    m_borederColor.a = float(color.A) / 255;
}

bool SimpleFillBorderedStyle::prepareData(
        const Matrix4& msMatrix, const Matrix4& vsMatrix)
{
    if (!Style::prepareData(msMatrix, vsMatrix))
        return false;

    if (m_normalId == -1)
        m_normalId = glGetAttribLocation(m_program->id(), "a_normal");

    if (m_isBorderId == -1)
        m_isBorderId = glGetUniformLocation(m_program->id(), "u_isBorder");
    ngsCheckGLEerror(glUniform1i(m_isBorderId, 1));

    if (m_vBorderWidthId == -1)
        m_vBorderWidthId =
                glGetUniformLocation(m_program->id(), "u_vBorderWidth");
    ngsCheckGLEerror(glUniform1f(m_vBorderWidthId, m_borderWidth));

    if (m_borderColorId == -1)
        m_borderColorId =
                glGetUniformLocation(m_program->id(), "u_borderColor");
    ngsCheckGLEerror(glUniform4f(m_borderColorId, m_borederColor.r,
            m_borederColor.g, m_borederColor.b, m_borederColor.a));

    return true;
}

void SimpleFillBorderedStyle::draw(const GlBuffer& buffer) const
{
    Style::draw(buffer);

    ngsCheckGLEerror(glEnableVertexAttribArray(m_mPositionId));
    ngsCheckGLEerror(glVertexAttribPointer(
            m_mPositionId, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0));

    ngsCheckGLEerror(glEnableVertexAttribArray(m_normalId));
    ngsCheckGLEerror(glVertexAttribPointer(m_normalId, 2, GL_FLOAT, GL_FALSE,
            5 * sizeof(float),
            reinterpret_cast<const GLvoid*>(3 * sizeof(float))));

    ngsCheckGLEerror(glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER, buffer.getGlHardBufferId(BF_INDICES)));
    ngsCheckGLEerror(glUniform1i(m_isBorderId, false));
    ngsCheckGLEerror(glDrawElements(
            GL_TRIANGLES, buffer.getIndexBufferSize(), GL_UNSIGNED_SHORT, 0));

    ngsCheckGLEerror(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
            buffer.getGlHardBufferId(BF_BORDER_INDICES)));
    ngsCheckGLEerror(glUniform1i(m_isBorderId, true));
    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES,
            buffer.getIndexBufferSize(BF_BORDER_INDICES), GL_UNSIGNED_SHORT,
            0));
}
