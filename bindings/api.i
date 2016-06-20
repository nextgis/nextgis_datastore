/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualisation support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2016 NextGIS, <info@nextgis.com>
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 32 of the License, or
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
 
/* File : api.i */

%include "arrays_java.i";
%include "typemaps.i"

// Typemaps for (void * nioBuffer)
%typemap(in, numinputs=1) (void * nioBuffer)
{
    if ($input == 0)
    {
        SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null array");
        return $null;
    }
    $1 = jenv->GetDirectBufferAddress($input);
    if ($1 == NULL)
    {
        SWIG_JavaThrowException(jenv, SWIG_JavaRuntimeException,
                                "Unable to get address of direct buffer. Buffer must be allocated direct.");
        return $null;
    }
}


/* These 3 typemaps tell SWIG what JNI and Java types to use */
%typemap(jni) (void * nioBuffer)  "jobject"
%typemap(jtype) (void * nioBuffer)  "java.nio.ByteBuffer"
%typemap(jstype) (void * nioBuffer)  "java.nio.ByteBuffer"
%typemap(javain) (void * nioBuffer)  "$javainput"
%typemap(javaout) (void * nioBuffer) {
    return $jnicall;
  }

%module api

#ifdef SWIGJAVA

#endif

#ifdef SWIGPYTHON

#endif

%inline %{
extern int ngsGetVersion(const char* request);
extern const char* ngsGetVersionString(const char* request);
extern int ngsInit(const char* path, const char* dataPath, const char* cachePath);
extern void ngsUninit();
extern int ngsDestroy(const char* path, const char* cachePath);
extern int ngsInitMap(const char* name, void * nioBuffer, int width, int height);
%}

#ifdef SWIGJAVA

%pragma(java) jniclasscode=%{
  static {
    try {
        System.loadLibrary("ngstore");
        System.loadLibrary("ngstoreapi");
    } catch (UnsatisfiedLinkError e) {
      System.err.println("Native code library failed to load. \n" + e);
      System.exit(1);
    }
  }
%}

#endif
