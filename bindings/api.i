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

%module Api

#ifdef SWIGJAVA
%{
#include <cstdio> // needed for declarations in extend_java.i
%}
%include typemap_java.i
%include callback_typemap_java.i
%include directory_container_typemap_java.i
#endif

%{
#include "api.h"
%}

%include "../src/common.h"
%include "../src/api.h"

#ifdef SWIGJAVA
%include extend_java.i
%include callback_java.i
%include directory_container_java.i
#endif

#ifdef SWIGPYTHON

#endif

//%clear //const char *request, const char *name, const char* path,
//const char* cachePath, const char* dataPath, const char* description;

#ifdef SWIGJAVA

// exampe of adding import declaration
//%pragma(java) jniclassimports=%{
//import com.nextgis.store.NotificationCallback;
//%}

// exampe of adding import declaration
//%pragma(java) moduleimports=%{
//import com.nextgis.store.NotificationCallback;
//%}

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

// exampe of adding fuction to Api class
//%pragma(java) modulecode=%{
//  public static int ngsInitMap2(String name, java.nio.ByteBuffer nioBuffer, int width, int height) {
//    if(null == nioBuffer)
//       throw new NullPointerException("Needed not null buffer");
//    if(width * height > nioBuffer.capacity())
//        throw new IllegalArgumentException("Output buffer too small. Should be greater than width * height");
//    return ngsInitMap(name, nioBuffer, width, height);
//  }
//%}

#endif
