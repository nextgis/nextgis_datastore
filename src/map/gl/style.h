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
#include "image.h"
#include "matrix.h"
#include "program.h"

#include "ngstore/api.h"
#include "util/jsondocument.h"

namespace ngs
{

//------------------------------------------------------------------------------
// Style
//------------------------------------------------------------------------------

class Style : public GlObject
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
    virtual bool load(const JSONObject &store) = 0;
    virtual JSONObject save() const = 0;
    virtual const char* name() = 0;

    //static
public:
    static Style* createStyle(const char* name);

protected:
    virtual const GLchar* getShaderSource(enum ShaderType type);

protected:
    const GLchar* m_vertexShaderSource;
    const GLchar* m_fragmentShaderSource;
    GlProgram m_program;

    // GlObject interface
public:
    virtual void bind() override {}
    virtual void rebind() const override {}
    virtual void destroy() override { m_program.destroy(); } // NOTE: Destroy only style stored GlObjects (i.e. program)
};

typedef std::shared_ptr<Style> StylePtr;

//------------------------------------------------------------------------------
// SimpleVectorStyle
//------------------------------------------------------------------------------

class SimpleVectorStyle : public Style
{
public:
    SimpleVectorStyle();
    virtual void setColor(const ngsRGBA& color) { m_color = ngsRGBA2Gl(color); }

    // Style interface
public:
    virtual bool prepare(const Matrix4 &msMatrix, const Matrix4 &vsMatrix) override;
    virtual bool load(const JSONObject &store) override;
    virtual JSONObject save() const override;

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

    enum SimplePointStyle::PointType type() const { return m_type; }
    void setType(enum SimplePointStyle::PointType type) { m_type = type; }

    float size() const { return m_size; }
    void setSize(float size) { m_size = size; }

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix) override;
    virtual void draw(const GlBuffer& buffer) const override;
    virtual bool load(const JSONObject &store) override;
    virtual JSONObject save() const override;
    virtual const char *name() override { return "simplePoint"; }

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

    float lineWidth() const { return m_lineWidth; }
    void setLineWidth(float lineWidth) { m_lineWidth = lineWidth * 0.25f; }

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix) override;
    virtual void draw(const GlBuffer& buffer) const override;
    virtual bool load(const JSONObject &store) override;
    virtual JSONObject save() const override;
    virtual const char *name() override { return "simpleLine"; }

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
    virtual const char *name() override { return "simpleFill"; }
};

//------------------------------------------------------------------------------
// SimpleFillBorderedStyle
//------------------------------------------------------------------------------

class SimpleFillBorderedStyle : public SimpleVectorStyle
{
public:
    SimpleFillBorderedStyle();

    float borderWidth() const { return m_borderWidth; }
    void setBorderWidth(float borderWidth) { m_borderWidth = borderWidth; }
    void setBorderColor(const ngsRGBA& color) { m_borderColor = ngsRGBA2Gl(color); }
//    void setBorderIndicesCount(size_t count);

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix) override;
    virtual void draw(const GlBuffer& buffer) const override;
    virtual bool load(const JSONObject &store) override;
    virtual JSONObject save() const override;
    virtual const char *name() override { return "simpleFillBordered"; }

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
    void setImage(GlImage * const image) { m_image = image; }

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix) override;
    virtual void draw(const GlBuffer& buffer) const override;

protected:
    GlImage *m_image;

    // Style interface
public:
    virtual bool load(const JSONObject &/*store*/) override { return true; }
    virtual JSONObject save() const override { return JSONObject(); }
    virtual const char *name() override { return "simpleImage"; }
};


}  // namespace ngs

#endif  // NGSSTYLE_H
