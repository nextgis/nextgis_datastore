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

%include "typemaps.i"

%module Api

// from SWIG's docs "Converting Java String arrays to char **"
// http://www.swig.org/Doc3.0/Java.html#Java_converting_java_string_arrays

// This tells SWIG to treat char** as a special case when used as a parameter in a function call
%typemap(in) char** (jint size) {
    int i = 0;
    size = jenv->GetArrayLength($input);
    $1 = (char**) malloc((size + 1) * sizeof(char*));
    // make a copy of each string
    for (i = 0; i < size; ++i) {
        jstring j_string = (jstring) jenv->GetObjectArrayElement($input, i);
        const char* c_string = jenv->GetStringUTFChars(j_string, 0);
        $1[i] = (char*) malloc(strlen((c_string) + 1) * sizeof(const char*));
        strcpy($1[i], c_string);
        jenv->ReleaseStringUTFChars(j_string, c_string);
        jenv->DeleteLocalRef(j_string);
    }
    $1[i] = 0;
}

// This cleans up the memory we malloc'd before the function call
%typemap(freearg) char** {
    int i;
    for (i = 0; i < size$argnum - 1; ++i) {
      free($1[i]);
    }
    free($1);
}

// This allows a C function to return a char** as a Java String array
%typemap(out) char** {
    int i;
    int len = 0;
    jstring temp_string;
    const jclass clazz = jenv->FindClass("java/lang/String");

    while ($1[len]) ++len;
    jresult = jenv->NewObjectArray(len, clazz, NULL);
    // TODO: exception checking omitted

    for (i = 0; i < len; ++i) {
      temp_string = jenv->NewStringUTF(*result++);
      jenv->SetObjectArrayElement(jresult, i, temp_string);
      jenv->DeleteLocalRef(temp_string);
    }
}

// These 3 typemaps tell SWIG what JNI and Java types to use
%typemap(jni) char** "jobjectArray"
%typemap(jtype) char** "String[]"
%typemap(jstype) char** "String[]"

// These 2 typemaps handle the conversion of the jtype to jstype typemap type and visa versa
%typemap(javain) char** "$javainput"
%typemap(javaout) char** {
    return $jnicall;
}
