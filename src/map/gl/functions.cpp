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


void prepareContext()
{
    // FIXME: Is this Mac OS X specific?
    ngsCheckGLError(glEnable(GL_PROGRAM_POINT_SIZE_EXT));
    ngsCheckGLError(glEnable(GL_MULTISAMPLE));
// NOTE: In usual cases no need in depth test
//    ngsCheckGLError(glEnable(GL_DEPTH_TEST));
//    ngsCheckGLError(glDepthFunc(GL_LEQUAL));
//    ngsCheckGLError(glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST));
//    ngsCheckGLError(glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
    ngsCheckGLError(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    ngsCheckGLError(glEnable(GL_BLEND));
}

GlObject::GlObject() : m_bound(false)
{
}

} // namespace ngs
