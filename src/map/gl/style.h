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

#include "buffer.h"
#include "matrix.h"
#include "program.h"

#include "ngstore/api.h"

namespace ngs
{

//------------------------------------------------------------------------------
// Style
//------------------------------------------------------------------------------

class Style
{
public:
    enum ShaderType {
        SH_VERTEX,
        SH_FRAGMENT
    };
public:
    Style();
    virtual ~Style() = default;
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix);
    virtual void draw(const GlBuffer& buffer) const;

protected:
    virtual const GLchar* getShaderSource(enum ShaderType type);

protected:
    const GLchar* m_vertexShaderSource;
    const GLchar* m_fragmentShaderSource;
    GlProgram m_program;
};

//------------------------------------------------------------------------------
// SimpleVectorStyle
//------------------------------------------------------------------------------

class SimpleVectorStyle : public Style
{
public:
    SimpleVectorStyle();
    virtual void setColor(const ngsRGBA& color);

    // Style interface
public:
    virtual bool prepare(const Matrix4 &msMatrix, const Matrix4 &vsMatrix) override;

protected:
    GlColor m_color;
};

//------------------------------------------------------------------------------
// SimplePointStyle
//------------------------------------------------------------------------------

class SimplePointStyle : public SimpleVectorStyle
{
public:
    enum PointType {
        PT_UNKNOWN = 0,
        PT_SQUARE,
        PT_RECTANGLE,
        PT_CIRCLE,
        PT_TRIANGLE,
        PT_DIAMOND,
        PT_STAR
    };
public:
    SimplePointStyle(enum PointType type = PT_CIRCLE);

    enum SimplePointStyle::PointType getType() const { return m_type; }
    void setType(enum SimplePointStyle::PointType type) { m_type = type; }

    float getSize() const { return m_size; }
    void setSize(float size) { m_size = size; }

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix) override;
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    enum PointType m_type;
    float m_size;
};

//------------------------------------------------------------------------------
// SimpleLineStyle
//------------------------------------------------------------------------------

class SimpleLineStyle : public SimpleVectorStyle
{
public:
    SimpleLineStyle();

    float getLineWidth() const { return m_lineWidth; }
    void setLineWidth(float lineWidth) { m_lineWidth = lineWidth * 0.25f; }

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix) override;
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    GLint m_normalId;
    GLint m_vLineWidthId;

    float m_lineWidth;
};

//------------------------------------------------------------------------------
// SimpleFillStyle
//------------------------------------------------------------------------------

class SimpleFillStyle : public SimpleVectorStyle
{
public:
    SimpleFillStyle();

    // Style interface
public:
    virtual void draw(const GlBuffer& buffer) const override;
    virtual bool prepare(const Matrix4 &msMatrix, const Matrix4 &vsMatrix) override;
};

//------------------------------------------------------------------------------
// SimpleFillBorderedStyle
//------------------------------------------------------------------------------

class SimpleFillBorderedStyle : public SimpleVectorStyle
{
public:
    SimpleFillBorderedStyle();

    float getBorderWidth() const { return m_borderWidth; }
    void setBorderWidth(float borderWidth) { m_borderWidth = borderWidth; }
    void setBorderColor(const ngsRGBA& color);
//    void setBorderIndicesCount(size_t count);

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix) override;
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    GlColor m_borderColor;
    float m_borderWidth;
};

//------------------------------------------------------------------------------
// SimpleImageStyle
//------------------------------------------------------------------------------

class SimpleImageStyle : public Style
{
public:
    SimpleImageStyle();
    void setImage(GLubyte * imageData, GLsizei width, GLsizei height) {
        m_imageData = imageData;
        m_width = width;
        m_height = height;
    }

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix) override;
    virtual void draw(const GlBuffer& buffer) const override;


};


}  // namespace ngs

#endif  // NGSSTYLE_H
