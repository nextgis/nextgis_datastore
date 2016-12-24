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

%module(directors="1") Api

%include "typemaps.i"
%include "enumtypeunsafe.swg" // for use in cases

%javaconst(1);

%ignore _ngsDirectoryEntry;
%ignore _ngsDirectoryContainer;
%ignore ngsGetDirectoryContainerPath;
%ignore ngsDestroyDirectoryContainerPath;
%ignore ngsGetDirectoryEntryPath;
%ignore ngsDestroyDirectoryEntryPath;

%ignore classEntry;
%ignore constructorEntry;
%ignore fieldBaseName;
%ignore fieldExtension;
%ignore fieldType;

%ignore classContainer;
%ignore constructorContainer;
%ignore fieldEntries;
%ignore fieldParentPath;
%ignore fieldDirectoryName;

%rename (DirectoryEntryType) ngsDirectoryEntryType;


%feature("director") DirectoryContainerLoadCallback;

%typemap(in) (ngsDirectoryContainerLoadCallback callback = NULL, void* callbackArguments = NULL)
{
    // init DirectoryEntry constructor and fields
    if (!classEntry) {
        classEntry = reinterpret_cast<jclass>(jenv->NewGlobalRef(jenv->FindClass("com/nextgis/store/bindings/DirectoryEntry")));
    }
    if (!constructorEntry) {
        constructorEntry = jenv->GetMethodID(classEntry, "<init>", "()V");
    }
    if (!fieldBaseName) {
        fieldBaseName = jenv->GetFieldID(classEntry, "mBaseName", "Ljava/lang/String;");
    }
    if (!fieldExtension) {
        fieldExtension = jenv->GetFieldID(classEntry, "mExtension", "Ljava/lang/String;");
    }
    if (!fieldType) {
        fieldType = jenv->GetFieldID(classEntry, "mType", "I");
    }

    // init DirectoryContainer constructor and fields
    if (!classContainer) {
        classContainer = reinterpret_cast<jclass>(jenv->NewGlobalRef(jenv->FindClass("com/nextgis/store/bindings/DirectoryContainer")));
    }
    if (!constructorContainer) {
        constructorContainer = jenv->GetMethodID(classContainer, "<init>", "()V");
    }
    if (!fieldEntries) {
        fieldEntries = jenv->GetFieldID(classContainer, "mEntries", "[Lcom/nextgis/store/bindings/DirectoryEntry;");
    }
    if (!fieldParentPath) {
        fieldParentPath = jenv->GetFieldID(classContainer, "mParentPath", "Ljava/lang/String;");
    }
    if (!fieldDirectoryName) {
        fieldDirectoryName = jenv->GetFieldID(classContainer, "mDirectoryName", "Ljava/lang/String;");
    }

    if ($input != 0) {
        $1 = JavaDirectoryContainerLoadProxy;
        $2 = (void *) $input;
    } else {
        $1 = NULL;
        $2 = NULL;
    }
}

%typemap(jni) (ngsDirectoryContainerLoadCallback callback = NULL, void* callbackArguments = NULL)  "jlong"
%typemap(jtype) (ngsDirectoryContainerLoadCallback callback = NULL, void* callbackArguments = NULL)  "long"
%typemap(jstype) (ngsDirectoryContainerLoadCallback callback = NULL, void* callbackArguments = NULL)  "DirectoryContainerLoadCallback"
%typemap(javain) (ngsDirectoryContainerLoadCallback callback = NULL, void* callbackArguments = NULL)  "DirectoryContainerLoadCallback.getCPtr($javainput)"
%typemap(javaout) (ngsDirectoryContainerLoadCallback callback = NULL, void* callbackArguments = NULL)
{
    return $jnicall;
}


%typemap(directorin,descriptor="Lcom/nextgis/store/bindings/DirectoryContainer;") ngsDirectoryContainer*
{
    int entryCount = $1->entryCount;
    jobjectArray jEntryArray = jenv->NewObjectArray(entryCount, classEntry, nullptr);

    for (int i = 0; i < entryCount; ++i) {
        ngsDirectoryEntry* entry = $1->entries + i;

        // construct DirectoryEntry object
        jobject jEntry = jenv->NewObject(classEntry, constructorEntry);
        // TODO: exception checking omitted

        // set DirectoryEntry.mBaseName
        jobject jBaseName = jenv->NewStringUTF(entry->baseName);
        jenv->SetObjectField(jEntry, fieldBaseName, jBaseName);
        jenv->DeleteLocalRef(jBaseName);

        // set DirectoryEntry.mExtension
        jobject jExtension = jenv->NewStringUTF(entry->extension);
        jenv->SetObjectField(jEntry, fieldExtension, jExtension);
        jenv->DeleteLocalRef(jExtension);

        // set DirectoryEntry.mType
        jint jType = entry->type;
        jenv->SetIntField(jEntry, fieldType, jType);

        // set array element
        jenv->SetObjectArrayElement(jEntryArray, i, jEntry);
        jenv->DeleteLocalRef(jEntry);
    }

    // construct DirectoryContainer object
    $input = jenv->NewObject(classContainer, constructorContainer);
    // TODO: exception checking omitted

    // set DirectoryContainer.mEntries
    jenv->SetObjectField($input, fieldEntries, jEntryArray);
    jenv->DeleteLocalRef(jEntryArray);

    // set DirectoryContainer.mParentPath
    jstring jParentPath = jenv->NewStringUTF($1->parentPath);
    jenv->SetObjectField($input, fieldParentPath, jParentPath);
    jenv->DeleteLocalRef(jParentPath);

    // set DirectoryContainer.mDirectoryName
    jstring jDirectoryName = jenv->NewStringUTF($1->directoryName);
    jenv->SetObjectField($input, fieldDirectoryName, jDirectoryName);
    jenv->DeleteLocalRef(jDirectoryName);
}

%typemap(javadirectorin) ngsDirectoryContainer* "$jniinput"


// These 3 typemaps tell SWIG what JNI and Java types to use
%typemap(jni) ngsDirectoryContainer* "jobject"
%typemap(jtype) ngsDirectoryContainer* "DirectoryContainer"
%typemap(jstype) ngsDirectoryContainer* "DirectoryContainer"

// These 2 typemaps handle the conversion of the jtype to jstype typemap type and visa versa
%typemap(javain) ngsDirectoryContainer* "$javainput"
%typemap(javaout) ngsDirectoryContainer* {
    return $jnicall;
}
