/******************************************************************************
 * Project: libngstore
 * Purpose: NextGIS store and visualization support library
 * Author:  Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016-2017 NextGIS, <info@nextgis.com>
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
#include "functions.h"

#include "util/error.h"

/* Links:
//https://mkonrad.net/2014/12/08/android-off-screen-rendering-using-egl-pixelbuffers.html
//http://stackoverflow.com/questions/214437/opengl-fast-off-screen-rendering
//http://stackoverflow.com/questions/14785007/can-i-use-opengl-for-off-screen-rendering/14796456#14796456
//https://gist.github.com/CartBlanche/1271517
//http://stackoverflow.com/questions/21151259/replacing-glreadpixels-with-egl-khr-image-base-for-faster-pixel-copy
//https://vec.io/posts/faster-alternatives-to-glreadpixels-and-glteximage2d-in-opengl-es
//https://www.khronos.org/registry/egl/sdk/docs/man/html/eglIntro.xhtml
//https://wiki.maemo.org/SimpleGL_example
//http://stackoverflow.com/questions/12906971/difference-from-eglcreatepbuffersurface-and-eglcreatepixmapsurface-with-opengl-e
//http://stackoverflow.com/questions/25504188/is-it-possible-to-use-pixmaps-on-android-via-java-api-for-gles

//https://solarianprogrammer.com/2013/05/13/opengl-101-drawing-primitives/
//http://www.glprogramming.com/red/chapter02.html
//https://www3.ntu.edu.sg/home/ehchua/programming/opengl/CG_Introduction.html
//https://www3.ntu.edu.sg/home/ehchua/programming/android/Android_3D.html
//https://www.opengl.org/sdk/docs/man2/xhtml/gluUnProject.xml
//https://www.opengl.org/sdk/docs/man2/xhtml/gluProject.xml

//https://github.com/libmx3/mx3/blob/master/src/event_loop.cpp

//https://www.mapbox.com/blog/drawing-antialiased-lines/
//https://github.com/afiskon/cpp-opengl-vbo-vao-shaders/blob/master/main.cpp
*/

namespace ngs {

bool checkGLError(const char *cmd) {
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        const char *error = nullptr;
        switch (err) {
            case GL_INVALID_ENUM:
                error = "INVALID_ENUM";
            break;
            case GL_INVALID_VALUE:
                error = "INVALID_VALUE";
            break;
            case GL_INVALID_OPERATION:
                error = "INVALID_OPERATION";
            break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
            case GL_OUT_OF_MEMORY:
                error = "OUT_OF_MEMORY";
            break;
#ifdef GL_STACK_UNDERFLOW
            case GL_STACK_UNDERFLOW:
                error = "STACK_UNDERFLOW";
            break;
#endif
#ifdef GL_STACK_OVERFLOW
            case GL_STACK_OVERFLOW:
                error = "STACK_OVERFLOW";
            break;
#endif
            default:
                error = "(unknown)";
        }

        return !errorMessage("%s: Error GL_%s", cmd, error);
    }
    return false;
}

void reportGlStatus(GLuint obj) {
    GLint length;
    ngsCheckGLError(glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length));
    GLchar *log = new GLchar[length];
    ngsCheckGLError(glGetShaderInfoLog(obj, length, &length, log));
    warningMessage(log);
    delete [] log;
}

#ifdef USE_EGL
bool checkEGLError(const char *cmd) {
    EGLint err = eglGetError();
    if(err != EGL_SUCCESS){
        const char *error = nullptr;
        switch (err) {
        case EGL_NOT_INITIALIZED:
            error = "NOT_INITIALIZED";
            break;
        case EGL_BAD_ACCESS:
            error = "BAD_ACCESS";
        break;
        case EGL_BAD_ALLOC:
            error = "BAD_ALLOC";
            break;
        case EGL_BAD_ATTRIBUTE:
            error = "BAD_ATTRIBUTE";
            break;
        case EGL_BAD_CONTEXT:
            error = "BAD_CONTEXT";
            break;
        case EGL_BAD_CONFIG:
            error = "BAD_CONFIG";
            break;
        case EGL_BAD_CURRENT_SURFACE:
            error = "BAD_CURRENT_SURFACE";
            break;
        case EGL_BAD_DISPLAY:
            error = "BAD_DISPLAY";
            break;
        case EGL_BAD_SURFACE:
            error = "BAD_SURFACE";
            break;
        case EGL_BAD_MATCH:
            error = "BAD_MATCH";
            break;
        case EGL_BAD_PARAMETER:
            error = "BAD_PARAMETER";
            break;
        case EGL_BAD_NATIVE_PIXMAP:
            error = "BAD_NATIVE_PIXMAP";
            break;
        case EGL_BAD_NATIVE_WINDOW:
            error = "BAD_NATIVE_WINDOW";
            break;
        case EGL_CONTEXT_LOST:
            error = "CONTEXT_LOST";
            break;
        }
        return !errorMessage("%s: Error EGL_%s", cmd, error);
    }
    return false;
}
#endif // USE_EGL


} // namespace ngs


/*
#include <algorithm>
#include "cpl_error.h"
#include "cpl_string.h"
#include "glview.h"
#include <iostream>
#include "map/glfillers.h"
#include "style.h"
#include "ngstore/util/constants.h"

#define MAX_VERTEX_BUFFER_SIZE 16383
#define MAX_INDEX_BUFFER_SIZE 16383
#define MAX_GLOBAL_VERTEX_BUFFER_SIZE 327180000
#define MAX_GLOBAL_INDEX_BUFFER_SIZE 327180000


void GlFuctions::testDrawPreserved(int colorId) const
{
    static bool isBuffersFilled = false;
    static GLuint    buffers[2];
    if(!isBuffersFilled) {
        isBuffersFilled = true;
        ngsCheckGLEerror(glGenBuffers(2, buffers)); // TODO: glDeleteBuffers

        ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));

        GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                               -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                                4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                              };

        ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices), vVertices, GL_STATIC_DRAW));

        ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));

        GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                               -2236992.0f, 3972353.0f, 0.5f,
                                5187591.0f, 4509961.0f, 0.5f
                              };

        ngsCheckGLEerror(glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices2), vVertices2, GL_STATIC_DRAW));
    }

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[0]));

    ngsCheckGLEerror(glUniform4f(colorId, 1.0f, 0.0f, 0.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));

    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    ngsCheckGLEerror(glBindBuffer(GL_ARRAY_BUFFER, buffers[1]));

    ngsCheckGLEerror(glUniform4f(colorId, 0.0f, 0.0f, 1.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 ));

    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

}

void GlFuctions::testDraw(int colorId) const
{
    GLfloat vVertices[] = {  0.0f,  0.0f, 0.0f,
                           -8236992.95426f, 4972353.09638f, 0.0f, // New York //-73.99416666f, 40.72833333f //
                            4187591.86613f, 7509961.73580f, 0.0f  // Moscow   //37.61777777f, 55.75583333f //
                          };

    ngsCheckGLEerror(glUniform4f(colorId, 1.0f, 0.0f, 0.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));

    GLfloat vVertices2[] = {  1000000.0f,  -500000.0f, -0.5f,
                           -2236992.0f, 3972353.0f, 0.5f,
                            5187591.0f, 4509961.0f, 0.5f
                          };

    ngsCheckGLEerror(glUniform4f(colorId, 0.0f, 0.0f, 1.0f, 1.0f));

    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                             vVertices2 ));
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));

    ngsCheckGLEerror(glDrawArrays ( GL_TRIANGLES, 0, 3 ));
}

void GlFuctions::drawPolygons(const vector<GLfloat> &vertices,
                              const vector<GLushort> &indices) const
{
    if(vertices.empty() || indices.empty ())
        return;
    ngsCheckGLEerror(glEnableVertexAttribArray ( 0 ));
    ngsCheckGLEerror(glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 0,
                                            vertices.data ()));
    ngsCheckGLEerror(glDrawElements(GL_TRIANGLES, indices.size (),
                                    GL_UNSIGNED_SHORT, indices.data ()));
}


*/
