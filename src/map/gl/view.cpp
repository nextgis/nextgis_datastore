/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*  Author: NikitaFeodonit, nfeodonit@yandex.com
*******************************************************************************
*  Copyright (C) 2016-2017 NextGIS, <info@nextgis.com>
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 2 of the License, or
*   (at your option) any later version.
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "view.h"

#include "style.h"

namespace ngs {

GlView::GlView() : MapView()
{

}

GlView::GlView(const CPLString &name, const CPLString &description,
               unsigned short epsg, const Envelope &bounds) :
    MapView(name, description, epsg, bounds)
{

}

void GlView::setBackgroundColor(const ngsRGBA &color)
{
    MapView::setBackgroundColor(color);
    m_glBkColor.r = float(color.R) / 255;
    m_glBkColor.g = float(color.G) / 255;
    m_glBkColor.b = float(color.B) / 255;
    m_glBkColor.a = float(color.A) / 255;
}

// NOTE: Should be run on OpenGL current context
void GlView::clearBackground()
{
    ngsCheckGLError(glClearColor(m_glBkColor.r, m_glBkColor.g, m_glBkColor.b,
                                  m_glBkColor.a));
    ngsCheckGLError(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}


bool GlView::draw(ngsDrawState state, const Progress &progress)
{
    clearBackground();

#ifdef NGS_GL_DEBUG
    // Test drawing without layers
//    testDrawPolygons();
//    testDrawPoints();
    testDrawLines();
#else
    if(m_layers.empty()) {
        return true;
    }
#endif // NGS_GL_DEBUG
    return true;
}

#ifdef NGS_GL_DEBUG
void GlView::testDrawPoints() const
{
    GlBuffer buffer1;
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(0);
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(1);
    buffer1.addVertex(4187591.86613f);
    buffer1.addVertex(7509961.73580f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(2);

//    GLfloat vVertices[] = { 0.0f,  0.0f, 0.0f,
//                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
//                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
//                          };

    GlBuffer buffer2;
    buffer2.addVertex(1000000.0f);
    buffer2.addVertex(-500000.0f);
    buffer2.addVertex(-0.5f);
    buffer2.addIndex(0);
    buffer2.addVertex(-2236992.0f);
    buffer2.addVertex(3972353.0f);
    buffer2.addVertex(0.5f);
    buffer2.addIndex(1);
    buffer2.addVertex(5187591.0f);
    buffer2.addVertex(4509961.0f);
    buffer2.addVertex(0.5f);
    buffer2.addIndex(2);

//    GLfloat vVertices2[] = { 1000000.0f,  -500000.0f, -0.5f,
//                            -2236992.0f, 3972353.0f, 0.5f,
//                             5187591.0f, 4509961.0f, 0.5f
//                           };


    SimplePointStyle style(SimplePointStyle::PT_CIRCLE);


    buffer2.bind();
    style.setColor({0, 0, 0, 255});
    style.setSize(27.5f);
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer2);
    style.setColor({255, 0, 0, 255});
    style.setSize(25.0f);
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer2);


    style.setColor({0, 0, 0, 255});
    style.setSize(18.5f);
    style.setType(SimplePointStyle::PT_TRIANGLE);
    buffer1.bind();
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer1);
    style.setColor({0, 0, 255, 255});
    style.setSize(16.0f);
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer1);

    buffer2.destroy();
    buffer1.destroy();
}

void GlView::testDrawPolygons() const
{
    GlBuffer buffer1;
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(0);
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(1);
    buffer1.addVertex(4187591.86613f);
    buffer1.addVertex(7509961.73580f);
    buffer1.addVertex(0.0f);
    buffer1.addIndex(2);

//    GLfloat vVertices[] = { 0.0f,  0.0f, 0.0f,
//                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
//                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
//                          };

    GlBuffer buffer2;
    buffer2.addVertex(1000000.0f);
    buffer2.addVertex(-500000.0f);
    buffer2.addVertex(-0.5f);
    buffer2.addIndex(0);
    buffer2.addVertex(-2236992.0f);
    buffer2.addVertex(3972353.0f);
    buffer2.addVertex(0.5f);
    buffer2.addIndex(1);
    buffer2.addVertex(5187591.0f);
    buffer2.addVertex(4509961.0f);
    buffer2.addVertex(0.5f);
    buffer2.addIndex(2);

//    GLfloat vVertices2[] = { 1000000.0f,  -500000.0f, -0.5f,
//                            -2236992.0f, 3972353.0f, 0.5f,
//                             5187591.0f, 4509961.0f, 0.5f
//                           };


    SimpleFillStyle style;

    style.setColor({255, 0, 0, 255});
    buffer2.bind();
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer2);

    style.setColor({0, 0, 255, 255});
    buffer1.bind();
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer1);

    buffer2.destroy();
    buffer1.destroy();
}

void GlView::testDrawLines() const
{
    OGRPoint pt1(0.0, 0.0);
    OGRPoint pt2(-8236992.95426, 4972353.09638);
//    OGRPoint pt2(1000000.0, 1000000.0);
    Normals normals = ngsGetNormals(pt1, pt2);

    GlBuffer buffer1;
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(normals[1].x);
//    buffer1.addVertex(normals[1].y);
//    buffer1.addIndex(0);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(normals[0].x);
//    buffer1.addVertex(normals[0].y);
//    buffer1.addIndex(1);

//    buffer1.addVertex(1000000.0f);
//    buffer1.addVertex(1000000.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(normals[1].x);
//    buffer1.addVertex(normals[1].y);
//    buffer1.addIndex(2);

//    buffer1.addVertex(1000000.0f);
//    buffer1.addVertex(1000000.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(normals[0].x);
//    buffer1.addVertex(normals[0].y);
//    buffer1.addIndex(1);
//    buffer1.addIndex(2);
//    buffer1.addIndex(3);

//    buffer1.addVertex(10000.0f);
//    buffer1.addVertex(10000.0f);
//    buffer1.addVertex(0.0f);
//    buffer1.addVertex(normals[3].x);
//    buffer1.addVertex(normals[3].y);
//    buffer1.addIndex(1);
//    buffer1.addIndex(2);
//    buffer1.addIndex(3);

    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(normals.right.x);
    buffer1.addVertex(normals.right.y);
    buffer1.addIndex(0);
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(normals.right.x);
    buffer1.addVertex(normals.right.y);
    buffer1.addIndex(1);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(normals.left.x);
    buffer1.addVertex(normals.left.y);
    buffer1.addIndex(2);
    buffer1.addVertex(-8236992.95426f);
    buffer1.addVertex(4972353.09638f);
    buffer1.addVertex(0.0f);
    buffer1.addVertex(normals.left.x);
    buffer1.addVertex(normals.left.y);
    buffer1.addIndex(1);
    buffer1.addIndex(2);
    buffer1.addIndex(3);


    //    buffer1.addVertex(4187591.86613f);
    //    buffer1.addVertex(7509961.73580f);
    //    buffer1.addVertex(0.0f);
    //    buffer1.addIndex(2);

    //    GLfloat vVertices[] = { 0.0f,  0.0f, 0.0f,
    //                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
    //                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
    //                          };

    //    GlBuffer buffer2;
    //    buffer2.addVertex(1000000.0f);
    //    buffer2.addVertex(-500000.0f);
    //    buffer2.addVertex(-0.5f);
    //    buffer2.addIndex(0);
    //    buffer2.addVertex(-2236992.0f);
    //    buffer2.addVertex(3972353.0f);
    //    buffer2.addVertex(0.5f);
    //    buffer2.addIndex(1);
    //    buffer2.addVertex(5187591.0f);
    //    buffer2.addVertex(4509961.0f);
    //    buffer2.addVertex(0.5f);
    //    buffer2.addIndex(2);

    //    GLfloat vVertices2[] = { 1000000.0f,  -500000.0f, -0.5f,
    //                            -2236992.0f, 3972353.0f, 0.5f,
    //                             5187591.0f, 4509961.0f, 0.5f
    //                           };


    SimpleLineStyle style;
    style.setLineWidth(15.0f);
    style.setColor({255, 0, 0, 255});
//    buffer2.bind();
//    style.prepare(getSceneMatrix(), getInvViewMatrix());
//    style.draw(buffer2);

//    style.setColor({0, 0, 255, 255});
    buffer1.bind();
    style.prepare(getSceneMatrix(), getInvViewMatrix());
    style.draw(buffer1);

//    buffer2.destroy();
    buffer1.destroy();
}


#endif // NGS_GL_DEBUG

} // namespace ngs


