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
 
%include "arrays_java.i";
%include "typemaps.i"

%module Api
%{
#include <cstdio>
%}


// Do we need typemap to android or java.awt Color?
%rename (Color) _ngsRGBA;

typedef struct _ngsRGBA  {
    unsigned char R;
    unsigned char G;
    unsigned char B;
    unsigned char A;
}ngsRGBA;

%extend _ngsRGBA {
   char *toString() {
       static char tmp[1024];
       sprintf(tmp,"RGBS (%g, %g, %g, %g)", self->R, self->G, self->B, self->A);
       return tmp;
   }
   _ngsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
       ngsRGBA *rgba = (ngsRGBA *) malloc(sizeof(ngsRGBA));
       rgba->R = r;
       rgba->G = g;
       rgba->B = b;
       rgba->A = a;
       return rgba;
   }
};

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
