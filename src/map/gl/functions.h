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
    #define MAIN_FRAMEBUFFER 1 // 0 - back, 1 - front.
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
    #define MAIN_FRAMEBUFFER 0
    #define GLES
    #define GL_GLEXT_PROTOTYPES 1
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    #define MAIN_FRAMEBUFFER 1
    #define GL_GLEXT_PROTOTYPES
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
//    #include <GL/gl.h>
//    #include <GL/glext.h>
#endif

#ifdef USE_EGL // need for headless desktop drawing (i.e. some preview generation)
#include "EGL/egl.h"
#endif // USE_EGL

#include <memory>

#include "api_priv.h"

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

#if defined GLES || defined __linux__
#define glClearDepth glClearDepthf
#define glDepthRange glDepthRangef
#endif //GLES

typedef struct _glcolor {
    float r;
    float g;
    float b;
    float a;
} GlColor;

inline GlColor ngsRGBA2Gl(const ngsRGBA& color) {
    return  { float(color.R) / 255.0f, float(color.G) / 255.0f,
                float(color.B) / 255.0f, float(color.A) / 255.0f };
}

inline ngsRGBA ngsGl2RGBA(const GlColor &color) {
    return { static_cast<unsigned char>(color.r * 255.0f),
             static_cast<unsigned char>(color.g * 255.0f),
             static_cast<unsigned char>(color.b * 255.0f),
             static_cast<unsigned char>(color.a * 255.0f) };
}

bool checkGLError(const char *cmd);
void reportGlStatus(GLuint obj);
void prepareContext();

/**
 * @brief The GlObject class Base class for Gl objects
 */
class GlObject
{
public:
    GlObject();
    virtual ~GlObject() = default;
    virtual void bind() = 0;
    virtual void rebind() const = 0;
    virtual bool bound() const { return m_bound; }
    virtual void destroy() = 0;

protected:
    bool m_bound;
};

using GlObjectPtr = std::shared_ptr<GlObject>;

}

#endif // NGSGLFUNCTIONS_H
