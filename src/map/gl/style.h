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
#include "program.h"

#include "map/matrix.h"
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

    enum Type {
        T_POINT,
        T_LINE,
        T_FILL,
        T_IMAGE
    };

public:
    Style();
    virtual ~Style() = default;
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                         enum GlBuffer::BufferType type);
    virtual void draw(const GlBuffer& buffer) const;
    virtual bool load(const JSONObject &store) = 0;
    virtual JSONObject save() const = 0;
    virtual const char* name() = 0;
    virtual enum Type type() const { return m_styleType; }

    //static
public:
    static Style* createStyle(const char* name);


    // GlObject interface
public:
    virtual void bind() override {}
    virtual void rebind() const override {}
    virtual void destroy() override { m_program.destroy(); } // NOTE: Destroy only style stored GlObjects (i.e. program)

protected:
    virtual const GLchar* getShaderSource(enum ShaderType type);

protected:
    const GLchar* m_vertexShaderSource;
    const GLchar* m_fragmentShaderSource;
    GlProgram m_program;
    enum Type m_styleType;
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
    virtual ngsRGBA color() const { return ngsGl2RGBA(m_color); }

    // Style interface
public:
    virtual bool prepare(const Matrix4 &msMatrix, const Matrix4 &vsMatrix,
                         enum GlBuffer::BufferType type) override;
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

    enum SimplePointStyle::PointType pointType() const { return m_type; }
    void setType(enum SimplePointStyle::PointType type) { m_type = type; }

    float size() const { return m_size; }
    void setSize(float size) { m_size = size; }

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                         enum GlBuffer::BufferType type) override;
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
enum CapType {
    CT_BUTT,
    CT_ROUND,
    CT_SQUARE
};

enum JoinType {
    JT_MITER,
    JT_ROUND,
    JT_BEVELED
};

class SimpleLineStyle : public SimpleVectorStyle
{
public:
    SimpleLineStyle();

    float width() const { return m_width; }
    void setWidth(float lineWidth) { m_width = lineWidth * 0.25f; }
    enum CapType capType() const;
    void setCapType(const CapType &capType);
    enum JoinType joinType() const;
    void setJoinType(const JoinType &joinType);
    unsigned char segmentCount() const;
    void setSegmentCount(unsigned char segmentCount);

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                         enum GlBuffer::BufferType type) override;
    virtual void draw(const GlBuffer& buffer) const override;
    virtual bool load(const JSONObject &store) override;
    virtual JSONObject save() const override;
    virtual const char *name() override { return "simpleLine"; }

protected:
    GLint m_normalId;
    GLint m_vLineWidthId;

    float m_width;
    enum CapType m_capType;
    enum JoinType m_joinType;
    unsigned char m_segmentCount;
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
    virtual bool prepare(const Matrix4 &msMatrix, const Matrix4 &vsMatrix,
                         enum GlBuffer::BufferType type) override;
    virtual const char *name() override { return "simpleFill"; }
};

//------------------------------------------------------------------------------
// SimpleFillBorderedStyle
//------------------------------------------------------------------------------

class SimpleFillBorderedStyle : public Style
{
public:
    SimpleFillBorderedStyle();

    float borderWidth() const { return m_line.width(); }
    void setBorderWidth(float borderWidth) { m_line.setWidth(borderWidth); }
    void setBorderColor(const ngsRGBA& color) { m_line.setColor(color); }
    void setColor(const ngsRGBA& color) { m_fill.setColor(color); }
    ngsRGBA color() const { return m_fill.color(); }
    enum CapType capType() const;
    void setCapType(const CapType &capType);
    enum JoinType joinType() const;
    void setJoinType(const JoinType &joinType);
    unsigned char segmentCount() const;
    void setSegmentCount(unsigned char segmentCount);

    // Style interface
public:
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                         enum GlBuffer::BufferType type) override;
    virtual void draw(const GlBuffer& buffer) const override;
    virtual bool load(const JSONObject &store) override;
    virtual JSONObject save() const override;
    virtual const char *name() override { return "simpleFillBordered"; }

protected:
    SimpleFillStyle m_fill;
    SimpleLineStyle m_line;
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
    virtual bool prepare(const Matrix4& msMatrix, const Matrix4& vsMatrix,
                         enum GlBuffer::BufferType type) override;
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
