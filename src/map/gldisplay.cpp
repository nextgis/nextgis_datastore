/******************************************************************************
*  Project: NextGIS GL Viewer
*  Purpose: GUI viewer for spatial data.
*  Author:  Dmitry Baryshnikov, bishop.dev@gmail.com
*******************************************************************************
*  Copyright (C) 2016 NextGIS, <info@nextgis.com>
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
#include "gldisplay.h"

#include <iostream>
#include "cpl_error.h"

using namespace ngs;

static GlDisplayPtr gGlDisplay = nullptr;
GlDisplayPtr ngs::getGlDisplay()
{
    if(gGlDisplay)
        return gGlDisplay;
    gGlDisplay.reset (new GlDisplay());
    if(gGlDisplay->init())
        return gGlDisplay;
    gGlDisplay.reset ();
    return gGlDisplay;
}

//------------------------------------------------------------------------------
// GlDisplay
//------------------------------------------------------------------------------

GlDisplay::GlDisplay() : m_eglDisplay(EGL_NO_DISPLAY)
{

}

GlDisplay::~GlDisplay()
{
    eglMakeCurrent ( m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
    eglTerminate ( m_eglDisplay );
    m_eglDisplay = EGL_NO_DISPLAY;
}

bool GlDisplay::init()
{
    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Get GL display failed.");
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize( m_eglDisplay, &major, &minor )) {
        CPLError(CE_Failure, CPLE_OpenFailed, "Get GL version failed.");
        return false;
    }

    if ((major <= 1) && (minor < 1)) {
        CPLError(CE_Failure, CPLE_NotSupported, "Unsupported GL version.");
        return false;
    }

    ngsCheckEGLEerror(eglBindAPI(EGL_OPENGL_ES_API));

    EGLint numConfigs = 0;
#ifdef _DEBUG
    cout << "Vendor: " << eglQueryString(m_eglDisplay, EGL_VENDOR) << endl;
    cout << "Version: " << eglQueryString(m_eglDisplay, EGL_VERSION) << endl;
    cout << "Client APIs: " << eglQueryString(m_eglDisplay, EGL_CLIENT_APIS) << endl;
    cout << "Client Extensions: " << eglQueryString(m_eglDisplay, EGL_EXTENSIONS) << endl;

    EGLConfig configs[10];
    EGLint id, red, depth, type;
    eglGetConfigs(m_eglDisplay, configs, 10, &numConfigs);
       cout << "Got " << numConfigs << " EGL configs:" << endl;
       for (EGLint i = 0; i < numConfigs; ++i) {
          eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_CONFIG_ID, &id);
          eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_SURFACE_TYPE, &type);
          eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_RED_SIZE, &red);
          eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_DEPTH_SIZE, &depth);
          cout << id <<
                  " Type = " << type <<
                  " Red Size = " << red <<
                  " Depth Size = " << depth <<
                  endl;
       }
#endif // _DEBUG

    // EGL config attributes
    const EGLint confAttr[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,    // very important!
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,          // we will create a pixelbuffer surface
        EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,     // if you need the alpha channel
        EGL_DEPTH_SIZE, 16,    // if you need the depth buffer
        EGL_NONE
    };
    numConfigs = 0;

    // choose the first config, i.e. best config
    if(!eglChooseConfig(m_eglDisplay, confAttr, &m_eglConf, 1, &numConfigs) || numConfigs != 1){
        CPLError(CE_Failure, CPLE_OpenFailed, "Choose GL config failed.");
        return false;
    }

#ifdef _DEBUG
      eglGetConfigAttrib(m_eglDisplay, m_eglConf, EGL_CONFIG_ID, &id);
      eglGetConfigAttrib(m_eglDisplay, m_eglConf, EGL_SURFACE_TYPE, &type);
      eglGetConfigAttrib(m_eglDisplay, m_eglConf, EGL_RED_SIZE, &red);
      eglGetConfigAttrib(m_eglDisplay, m_eglConf, EGL_DEPTH_SIZE, &depth);
      cout << "Selected config: " <<
              " Type = " << type <<
              " Red Size = " << red <<
              " Depth Size = " << depth <<
              endl;
#endif // _DEBUG
    return true;
}

EGLDisplay GlDisplay::eglDisplay() const
{
    return m_eglDisplay;
}

EGLConfig GlDisplay::eglConf() const
{
    return m_eglConf;
}

//------------------------------------------------------------------------------
bool ngs::checkEGLError(const char *cmd) {
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
        CPLError(CE_Failure, CPLE_AppDefined, "%s: Error EGL_%s", cmd, error);
        return true;
    }
    return false;
}

bool ngs::checkGLError(const char *cmd) {
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

        CPLError(CE_Failure, CPLE_AppDefined, "%s: Error GL_%s", cmd, error);

        return true;
    }
    return false;
}

void ngs::reportGlStatus(GLuint obj) {
    GLint length;
    ngsCheckGLEerror(glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &length));
    GLchar *log = new GLchar[length];
    ngsCheckGLEerror(glGetShaderInfoLog(obj, length, &length, log));
    CPLError(CE_Failure, CPLE_AppDefined, "%s", log);
    delete [] log;
}

