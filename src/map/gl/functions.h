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
#ifndef NGSGLFUNCTIONS_H
#define NGSGLFUNCTIONS_H

#if __APPLE__
    #include "TargetConditionals.h"
    #if TARGET_OS_IPHONE
        #define GLES
        #include <OpenGLES/ES2/gl.h>
        #include <OpenGLES/ES2/glext.h>
    #elif TARGET_IPHONE_SIMULATOR
        #include <OpenGLES/ES2/gl.h>
        #include <OpenGLES/ES2/glext.h>
    #elif TARGET_OS_MAC
        #include <OpenGL/OpenGL.h>
        #include <OpenGL/gl.h>
    #else
        #error Unsupported Apple platform
    #endif
#elif __ANDROID__
    #define GL_GLEXT_PROTOTYPES
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    #define GL_GLEXT_PROTOTYPES
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
//    #include <GL/gl.h>
//    #include <GL/glext.h>
#endif

#ifdef USE_EGL // need for headless desktop drawing (i.e. some preview generation)
#include "EGL/egl.h"
#endif // USE_EGL

namespace ngs {

#ifdef _DEBUG
#define ngsCheckGLError(cmd) {cmd; checkGLError(#cmd);}
#define ngsCheckEGLError(cmd) {cmd; checkEGLError(#cmd);}
#else
#define ngsCheckGLError(cmd) (cmd)
#define ngsCheckEGLError(cmd) (cmd)
#endif

#ifdef USE_EGL
bool checkEGLError(const char *cmd);
#endif // USE_EGL

typedef struct _glcolor {
    float r;
    float g;
    float b;
    float a;
} GlColor;

bool checkGLError(const char *cmd);
void reportGlStatus(GLuint obj);

}

#endif // NGSGLFUNCTIONS_H

/*

/*typedef int (*ngsBindVertexArray)(GLuint array);
typedef int (*ngsDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
typedef int (*ngsGenVertexArrays)(GLsizei n, GLuint* arrays);*//*

enum ngsShaderType {
    SH_VERTEX,
    SH_FRAGMENT
};

enum class LineCapType : uint8_t {
    Butt,
    Square,
    Round,
    // the following type is for internal use only
    FakeRound // TODO: remove it and switch to Round
};

enum class LineJoinType : uint8_t {
    Miter,
    Bevel,
    Round,
    // the following two types are for internal use only
    FlipBevel,
    FakeRound
};

*/
