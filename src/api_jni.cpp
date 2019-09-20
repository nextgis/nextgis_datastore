/******************************************************************************
 * Project:  libngstore
 * Purpose:  NextGIS store and visualization support library
 * Author: Dmitry Baryshnikov, dmitry.baryshnikov@nextgis.com
 ******************************************************************************
 *   Copyright (c) 2018-2019 NextGIS, <info@nextgis.com>
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

#include <jni.h>

// gdal
#include "cpl_json.h"
#include "cpl_string.h"

// std
#include <vector>

// project
#include "api_priv.h"
#include "ngstore/api.h"
#include "ngstore/common.h"


#define NGS_JNI_FUNC(type, name) extern "C" JNIEXPORT type JNICALL Java_com_nextgis_maplib_API_ ## name


constexpr jboolean NGS_JNI_TRUE = JNI_TRUE;
constexpr jboolean NGS_JNI_FALSE = JNI_FALSE;
static JavaVM *g_vm;

class jniString
{
public:
    jniString(JNIEnv *env, jstring str) : m_env(env), m_original(str) {
        jboolean isCopy;
        m_native = env->GetStringUTFChars(str, &isCopy);
    }

    ~jniString() {
        m_env->ReleaseStringUTFChars(m_original, m_native);
    }

    const char *c_str() const { return m_native; }

private:
    JNIEnv *m_env;
    const char *m_native;
    jstring m_original;
};

static jclass g_APIClass;
static jmethodID g_NotifyMid;
static jmethodID g_ProgressMid;
static jclass g_StringClass;
static jclass g_EnvelopeClass;
static jmethodID g_EnvelopeInitMid;
static jclass g_PointClass;
static jmethodID g_PointInitMid;
static jclass g_CatalogObjectInfoClass;
static jmethodID g_CatalogObjectInfoInitMid;
static jclass g_FieldClass;
static jmethodID g_FieldInitMid;
static jclass g_DateComponentsClass;
static jmethodID g_DateComponentsInitMid;
static jclass g_EditOperationClass;
static jmethodID g_EditOperationInitMid;
static jclass g_RequestResultClass;
static jmethodID g_RequestResultInitMid;
static jclass g_RequestResultJsonClass;
static jmethodID g_RequestResultJsonInitMid;
static jclass g_RequestResultRawClass;
static jmethodID g_RequestResultRawInitMid;
static jclass g_AttachmentClass;
static jmethodID g_AttachmentInitMid;
static jclass g_RGBAClass;
static jmethodID g_RGBAInitMid;
static jclass g_TouchResultClass;
static jmethodID g_TouchResultInitMid;
static jclass g_QMSItemClass;
static jmethodID g_QMSItemInitMid;
static jclass g_QMSItemPropertiesClass;
static jmethodID g_QMSItemPropertiesInitMid;
static jclass g_TrackInfoClass;
static jmethodID g_TrackInfoInitMid;

//static jclass g_Class;
//static jmethodID g_InitMid;

static void notifyProxyFunc(const char *uri, enum ngsChangeCode operation)
{
    JNIEnv * g_env;
    // double check it's all ok
    int getEnvStat = g_vm->GetEnv((void **)&g_env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&g_env, nullptr) != 0) {
            return;
        }
    } else if (getEnvStat == JNI_EVERSION) {
        return;
    }

    jstring stringUri = g_env->NewStringUTF(uri);
    g_env->CallStaticVoidMethod(g_APIClass, g_NotifyMid, stringUri, operation);
    g_env->DeleteLocalRef(stringUri);

    if (g_env->ExceptionCheck()) {
        g_env->ExceptionDescribe();
    }

    g_vm->DetachCurrentThread();
}

static int progressProxyFunc(enum ngsCode status, double complete,
                             const char *message, void *progressArguments)
{
    JNIEnv *g_env;
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_6;
    args.name = "CPPJava";
    args.group = nullptr;

    // double check it's all ok
    int getEnvStat = g_vm->GetEnv((void **)&g_env, args.version);
    bool isMainThread = true;
    if(getEnvStat == JNI_EDETACHED && g_env == nullptr) {
        if(g_vm->AttachCurrentThread(&g_env, &args) != 0) {
            return 1;
        }
        isMainThread = false;
    }

    jstring stringMessage = g_env->NewStringUTF(message);
    auto progressArgumentsDigit = reinterpret_cast<long long>(progressArguments);
    jint res = g_env->CallStaticIntMethod(g_APIClass, g_ProgressMid, status, complete, stringMessage,
                                          static_cast<jint>(progressArgumentsDigit));
    g_env->DeleteLocalRef(stringMessage);

    if (g_env->ExceptionCheck()) {
        g_env->ExceptionDescribe();
    }

    if(!isMainThread) {
        g_vm->DetachCurrentThread();
    }

    return res;
}

static char **toOptions(JNIEnv *env, jobjectArray optionsArray)
{
    int count = env->GetArrayLength(optionsArray);
    char **nativeOptions = nullptr;
    for (int i = 0; i < count; ++i) {
        auto option = reinterpret_cast<jstring>(env->GetObjectArrayElement(optionsArray, i));
        nativeOptions = CSLAddString(nativeOptions, jniString(env, option).c_str());
    }

    return nativeOptions;
}

static jobjectArray fromOptions(JNIEnv *env, CSLConstList options)
{
    int count = CSLCount(options);
    jobjectArray result = env->NewObjectArray(count, g_StringClass, env->NewStringUTF(""));
    for(int i = 0; i < count; ++i) {
        env->SetObjectArrayElement(result, i, env->NewStringUTF(options[i]));
    }
    return result;
}

NGS_JNI_FUNC(jint, getVersion)(JNIEnv *env, jobject thisObj, jstring request)
{
    ngsUnused(thisObj);
    return ngsGetVersion(jniString(env, request).c_str());
}

NGS_JNI_FUNC(jstring, getVersionString)(JNIEnv *env, jobject thisObj, jstring request)
{
    ngsUnused(thisObj);
    const char *result = ngsGetVersionString(jniString(env, request).c_str());
    return env->NewStringUTF(result);
}

NGS_JNI_FUNC(void, unInit)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    ngsUnInit();

    env->DeleteGlobalRef(g_APIClass);
    env->DeleteGlobalRef(g_StringClass);
    env->DeleteGlobalRef(g_PointClass);
    env->DeleteGlobalRef(g_EnvelopeClass);
    env->DeleteGlobalRef(g_CatalogObjectInfoClass);
    env->DeleteGlobalRef(g_FieldClass);
    env->DeleteGlobalRef(g_EditOperationClass);
    env->DeleteGlobalRef(g_RequestResultClass);
    env->DeleteGlobalRef(g_RequestResultJsonClass);
    env->DeleteGlobalRef(g_RequestResultRawClass);
    env->DeleteGlobalRef(g_AttachmentClass);
    env->DeleteGlobalRef(g_RGBAClass);
    env->DeleteGlobalRef(g_TouchResultClass);
    env->DeleteGlobalRef(g_QMSItemClass);
    env->DeleteGlobalRef(g_QMSItemPropertiesClass);
    env->DeleteGlobalRef(g_TrackInfoClass);
}

static bool getClassInitMethod(JNIEnv *env, const char *className, const char *signature,
                               jclass &classVar, jmethodID &methodVar)
{
    jclass clazz = env->FindClass(className);
    classVar = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
    methodVar = env->GetMethodID(classVar, "<init>", signature);

    return methodVar != nullptr;
}

NGS_JNI_FUNC(jboolean, init)(JNIEnv *env, jobject thisObj, jobjectArray optionsArray)
{
    ngsUnused(thisObj);
    env->GetJavaVM(&g_vm);

    char **nativeOptions = toOptions(env, optionsArray);
    int result = ngsInit(nativeOptions);
    CSLDestroy(nativeOptions);

    jclass clazz = env->FindClass("com/nextgis/maplib/API");
    g_APIClass = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));

    clazz = env->FindClass("java/lang/String");
    g_StringClass = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));

    // register callback function: notify
    g_NotifyMid = env->GetStaticMethodID(g_APIClass, "notifyBridgeFunction", "(Ljava/lang/String;I)V");
    if(g_NotifyMid == nullptr) {
        return NGS_JNI_FALSE;
    }

    // register callback function: progress
    g_ProgressMid = env->GetStaticMethodID(g_APIClass, "progressBridgeFunction", "(IDLjava/lang/String;I)I");
    if(g_ProgressMid == nullptr) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/Point", "(DD)V", g_PointClass, g_PointInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/Envelope", "(DDDD)V", g_EnvelopeClass, g_EnvelopeInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/CatalogObjectInfo", "(Ljava/lang/String;IJ)V", g_CatalogObjectInfoClass, g_CatalogObjectInfoInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/Field", "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V", g_FieldClass, g_FieldInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/DateComponents", "(IIIIIII)V", g_DateComponentsClass, g_DateComponentsInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/EditOperation", "(JJJJI)V", g_EditOperationClass, g_EditOperationInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/RequestResult", "(ILjava/lang/String;)V", g_RequestResultClass, g_RequestResultInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/RequestResultJsonInt", "(IJ)V", g_RequestResultJsonClass, g_RequestResultJsonInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/RequestResultRaw", "(I[B)V", g_RequestResultRawClass, g_RequestResultRawInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/Attachment", "(JJLjava/lang/String;Ljava/lang/String;Ljava/lang/String;JJ)V", g_AttachmentClass, g_AttachmentInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/RGBA", "(IIII)V", g_RGBAClass, g_RGBAInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/TouchResult", "(IZ)V", g_TouchResultClass, g_TouchResultInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/QMSItemInt", "(ILjava/lang/String;Ljava/lang/String;ILjava/lang/String;ILcom/nextgis/maplib/Envelope;I)V", g_QMSItemClass, g_QMSItemInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/QMSItemPropertiesInt", "(IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;IIIILjava/lang/String;Lcom/nextgis/maplib/Envelope;Z)V", g_QMSItemPropertiesClass, g_QMSItemPropertiesInitMid)) {
        return NGS_JNI_FALSE;
    }

    if(!getClassInitMethod(env, "com/nextgis/maplib/TrackInfoInt", "(Ljava/lang/String;JJJ)V", g_TrackInfoClass, g_TrackInfoInitMid)) {
        return NGS_JNI_FALSE;
    }

    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(void, freeResources)(JNIEnv *env, jobject thisObj, jboolean full)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFreeResources(static_cast<char>(full ? 1 : 0));
}

NGS_JNI_FUNC(jstring, getLastErrorMessage)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsGetLastErrorMessage());
}

NGS_JNI_FUNC(jstring, settingsGetString)(JNIEnv *env, jobject thisObj, jstring key, jstring defaultVal)
{
    ngsUnused(thisObj);
    const char *result = ngsSettingsGetString(jniString(env, key).c_str(),
                                              jniString(env, defaultVal).c_str());
    return env->NewStringUTF(result);
}

NGS_JNI_FUNC(void, settingsSetString)(JNIEnv *env, jobject thisObj, jstring key, jstring value)
{
    ngsUnused(thisObj);
    ngsSettingsSetString(jniString(env, key).c_str(), jniString(env, value).c_str());
}

NGS_JNI_FUNC(jboolean, backup)(JNIEnv *env, jobject thisObj, jstring name, jlong dstObj,
        jlongArray objects, jint callbackId)
{
    ngsUnused(thisObj);
    int size = env->GetArrayLength(objects);
    jboolean isCopy;
    jlong *objectsArray = env->GetLongArrayElements(objects, &isCopy);
    std::vector<long long> objectsStdArray;
    for(int i = 0; i < size; ++i) {
        objectsStdArray.push_back(objectsArray[i]);
    }
    objectsStdArray.push_back(0);

    int result;
    if(callbackId == 0) {
        result = ngsBackup(jniString(env, name).c_str(), reinterpret_cast<CatalogObjectH>(dstObj),
                           reinterpret_cast<CatalogObjectH *>(objectsStdArray.data()), nullptr,
                           nullptr);
    }
    else {
        result = ngsBackup(jniString(env, name).c_str(), reinterpret_cast<CatalogObjectH>(dstObj),
                           reinterpret_cast<CatalogObjectH *>(objectsStdArray.data()),
                           progressProxyFunc, reinterpret_cast<void *>(callbackId));
    }
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

/*
 * Proxy to GDAL functions
 */

NGS_JNI_FUNC(jstring, getCurrentDirectory)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsGetCurrentDirectory());
}

NGS_JNI_FUNC(jstring, formFileName)(JNIEnv *env, jobject thisObj, jstring path, jstring name, jstring extension)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsFormFileName(jniString(env, path).c_str(), jniString(env, name).c_str(),
                             jniString(env, extension).c_str()));
}

NGS_JNI_FUNC(void, free)(JNIEnv *env, jobject thisObj, jlong pointer)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFree(reinterpret_cast<void *>(pointer));
}

/*
 * Miscellaneous functions
 */

NGS_JNI_FUNC(jobject, URLRequest)(JNIEnv *env, jobject thisObj, jint type, jstring url,
        jobjectArray options, jint callbackId)
{
    ngsUnused(thisObj);
    ngsURLRequestResult *result;
    if(callbackId == 0) {
        result = ngsURLRequest(static_cast<ngsURLRequestType>(type), jniString(env, url).c_str(),
                               toOptions(env, options), nullptr, nullptr);
    }
    else {
        result = ngsURLRequest(static_cast<ngsURLRequestType>(type), jniString(env, url).c_str(),
                               toOptions(env, options), progressProxyFunc,
                               reinterpret_cast<void *>(callbackId));
    }
    int status = result->status;
    std::string value(reinterpret_cast<const char*>(result->data),
                      static_cast<unsigned long>(result->dataLen));
    jstring outStr = env->NewStringUTF(value.c_str());
    ngsURLRequestResultFree(result);

    jvalue args[2];
    args[0].i = status;
    args[1].l = outStr;
    return env->NewObjectA(g_RequestResultClass, g_RequestResultInitMid, args);
}

NGS_JNI_FUNC(jobject, URLRequestJson)(JNIEnv *env, jobject thisObj, jint type, jstring url,
        jobjectArray options, jint callbackId)
{
    ngsUnused(thisObj);
    ngsURLRequestResult *result;
    if(callbackId == 0) {
        result = ngsURLRequest(static_cast<ngsURLRequestType>(type), jniString(env, url).c_str(),
                               toOptions(env, options), nullptr, nullptr);
    }
    else {
        result = ngsURLRequest(static_cast<ngsURLRequestType>(type), jniString(env, url).c_str(),
                               toOptions(env, options), progressProxyFunc,
                               reinterpret_cast<void *>(callbackId));
    }
    int status = result->status;
    CPLJSONDocument doc;
    long handle = 0;
    if(doc.LoadMemory(result->data, result->dataLen)) {
        handle = reinterpret_cast<long>(new CPLJSONObject(doc.GetRoot()));
    }
    ngsURLRequestResultFree(result);

    jvalue args[2];
    args[0].i = status;
    args[1].j = handle;
    return env->NewObjectA(g_RequestResultJsonClass, g_RequestResultJsonInitMid, args);
}

NGS_JNI_FUNC(jobject, URLRequestRaw)(JNIEnv *env, jobject thisObj, jint type, jstring url,
        jobjectArray options, jint callbackId)
{
    ngsUnused(thisObj);
    ngsURLRequestResult *result;
    if(callbackId == 0) {
        result = ngsURLRequest(static_cast<ngsURLRequestType>(type),
                               jniString(env, url).c_str(), toOptions(env, options), nullptr,
                               nullptr);
    }
    else {
        result = ngsURLRequest(static_cast<ngsURLRequestType>(type),
                              jniString(env, url).c_str(), toOptions(env, options),
                              progressProxyFunc, reinterpret_cast<void *>(callbackId));
    }
    int status = result->status;
    jbyteArray barray = env->NewByteArray(result->dataLen);
    env->SetByteArrayRegion(barray, 0, result->dataLen,
                            reinterpret_cast<const jbyte *>(result->data));
    ngsURLRequestResultFree(result);

    jvalue args[2];
    args[0].i = status;
    args[1].l = barray;
    return env->NewObjectA(g_RequestResultRawClass, g_RequestResultRawInitMid, args);
}

NGS_JNI_FUNC(jobject, URLUploadFile)(JNIEnv *env, jobject thisObj, jstring path, jstring url,
                                     jobjectArray options, jint callbackId)
{
    ngsUnused(thisObj);
    ngsURLRequestResult *result;

    if(callbackId == 0) {
        result = ngsURLUploadFile(jniString(env, path).c_str(), jniString(env, url).c_str(),
                                  toOptions(env, options), nullptr, nullptr);
    }
    else {
        result = ngsURLUploadFile(jniString(env, path).c_str(), jniString(env, url).c_str(),
                                  toOptions(env, options), progressProxyFunc,
                                  reinterpret_cast<void *>(callbackId));
    }

    CPLJSONDocument doc;
    long handle = 0;
    int status = result->status;
    if(doc.LoadMemory(result->data, result->dataLen)) {
        handle = reinterpret_cast<long>(new CPLJSONObject(doc.GetRoot()));
    }
    ngsURLRequestResultFree(result);

    jvalue args[2];
    args[0].i = status;
    args[1].j = handle;
    return env->NewObjectA(g_RequestResultJsonClass, g_RequestResultJsonInitMid, args);
}

NGS_JNI_FUNC(jboolean, URLAuthAdd)(JNIEnv *env, jobject thisObj, jstring url, jobjectArray options)
{
    ngsUnused(thisObj);
    return ngsURLAuthAdd(jniString(env, url).c_str(), toOptions(env, options)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobject, URLAuthGet)(JNIEnv *env, jobject thisObj, jstring url)
{
    ngsUnused(thisObj);
    char **properties = ngsURLAuthGet(jniString(env, url).c_str());
    jobject ret = fromOptions(env, reinterpret_cast<CSLConstList>(properties));
    ngsListFree(properties);
    return ret;
}

NGS_JNI_FUNC(jboolean, URLAuthDelete)(JNIEnv *env, jobject thisObj, jstring url)
{
    ngsUnused(thisObj);
    return ngsURLAuthDelete(jniString(env, url).c_str()) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jstring, md5)(JNIEnv *env, jobject thisObj, jstring value)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsMD5(jniString(env, value).c_str()));
}

NGS_JNI_FUNC(jstring, getDeviceId)(JNIEnv *env, jobject thisObj, jboolean regenerate)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsGetDeviceId(regenerate == JNI_TRUE));
}

NGS_JNI_FUNC(jstring, generatePrivateKey)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsGeneratePrivateKey());
}

NGS_JNI_FUNC(jlong, jsonDocumentCreate)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsJsonDocumentCreate());
}

NGS_JNI_FUNC(void, jsonDocumentFree)(JNIEnv *env, jobject thisObj, jlong document)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsJsonDocumentFree(reinterpret_cast<JsonDocumentH>(document));
}

NGS_JNI_FUNC(jboolean, jsonDocumentLoadUrl)(JNIEnv *env, jobject thisObj, jlong document,
                                            jstring url, jobjectArray options, jint callbackId)
{
    ngsUnused(thisObj);
    char **nativeOptions = toOptions(env, options);
    int result;
    if(callbackId == 0) {
        result = ngsJsonDocumentLoadUrl(reinterpret_cast<JsonDocumentH>(document),
                                        jniString(env, url).c_str(), nativeOptions, nullptr, nullptr);
    }
    else {
        result = ngsJsonDocumentLoadUrl(reinterpret_cast<JsonDocumentH>(document),
                                        jniString(env, url).c_str(), nativeOptions,
                                        progressProxyFunc, reinterpret_cast<void *>(callbackId));
    }
    CSLDestroy(nativeOptions);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, jsonDocumentRoot)(JNIEnv *env, jobject thisObj, jlong document)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsJsonDocumentRoot(reinterpret_cast<JsonDocumentH>(document)));
}

NGS_JNI_FUNC(void, jsonObjectFree)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsJsonObjectFree(reinterpret_cast<JsonObjectH>(object));
}

NGS_JNI_FUNC(jint, jsonObjectType)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsJsonObjectType(reinterpret_cast<JsonObjectH>(object));
}

NGS_JNI_FUNC(jboolean, jsonObjectValid)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsJsonObjectValid(reinterpret_cast<JsonObjectH>(object)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jstring, jsonObjectName)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsJsonObjectName(reinterpret_cast<JsonObjectH>(object)));
}

NGS_JNI_FUNC(jlongArray, jsonObjectChildren)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    jlongArray result;
    JsonObjectH *out = ngsJsonObjectChildren(reinterpret_cast<JsonObjectH>(object));
    std::vector<long> obArray;
    int counter = 0;
    while(out[counter] != nullptr) {
        obArray.push_back(reinterpret_cast<long>(out[counter]));
        counter++;
    }
    ngsJsonObjectChildrenListFree(out);

    result = env->NewLongArray(static_cast<jsize>(obArray.size()));
    if(result) {
        env->SetLongArrayRegion(result, 0, static_cast<jsize>(obArray.size()),
                                reinterpret_cast<const jlong*>(obArray.data()));
    }

    return result;
}

NGS_JNI_FUNC(jstring, jsonObjectGetString)(JNIEnv *env, jobject thisObj, jlong object,
                                            jstring defaultValue)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsJsonObjectGetString(reinterpret_cast<JsonObjectH>(object),
                                  jniString(env, defaultValue).c_str()));
}

NGS_JNI_FUNC(jdouble, jsonObjectGetDouble)(JNIEnv *env, jobject thisObj, jlong object,
                                            jdouble defaultValue)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsJsonObjectGetDouble(reinterpret_cast<JsonObjectH>(object),
                                   defaultValue);
}

NGS_JNI_FUNC(jint, jsonObjectGetInteger)(JNIEnv *env, jobject thisObj, jlong object,
                                          jint defaultValue)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsJsonObjectGetInteger(reinterpret_cast<JsonObjectH>(object),
                                defaultValue);
}

NGS_JNI_FUNC(jlong, jsonObjectGetLong)(JNIEnv *env, jobject thisObj, jlong object,
                                          jlong defaultValue)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsJsonObjectGetLong(reinterpret_cast<JsonObjectH>(object),
                                defaultValue);
}

NGS_JNI_FUNC(jboolean, jsonObjectGetBool)(JNIEnv *env, jobject thisObj, jlong object,
                                                jboolean defaultValue)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsJsonObjectGetBool(reinterpret_cast<JsonObjectH>(object),
                                      defaultValue) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, jsonObjectGetArray)(JNIEnv *env, jobject thisObj, jlong object, jstring name)
{
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsJsonObjectGetArray(
            reinterpret_cast<JsonObjectH>(object), jniString(env, name).c_str()));
}

NGS_JNI_FUNC(jlong, jsonObjectGetObject)(JNIEnv *env, jobject thisObj, jlong object, jstring name)
{
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsJsonObjectGetObject(
            reinterpret_cast<JsonObjectH>(object), jniString(env, name).c_str()));
}

NGS_JNI_FUNC(jint, jsonArraySize)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsJsonArraySize(reinterpret_cast<JsonObjectH>(object));
}

NGS_JNI_FUNC(jlong, jsonArrayItem)(JNIEnv *env, jobject thisObj, jlong object, jint index)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsJsonArrayItem(reinterpret_cast<JsonObjectH>(object), index));
}

NGS_JNI_FUNC(jstring, jsonObjectGetStringForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                                 jstring name, jstring defaultValue)
{
    ngsUnused(thisObj);
    return env->NewStringUTF( ngsJsonObjectGetStringForKey(
            reinterpret_cast<JsonObjectH>(object), jniString(env, name).c_str(),
            jniString(env, defaultValue).c_str()) );
}

NGS_JNI_FUNC(jdouble, jsonObjectGetDoubleForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                                  jstring name, jdouble defaultValue)
{
    ngsUnused(thisObj);
    return ngsJsonObjectGetDoubleForKey(reinterpret_cast<JsonObjectH>(object),
                                         jniString(env, name).c_str(), defaultValue);
}

NGS_JNI_FUNC(jint, jsonObjectGetIntegerForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                               jstring name, jint defaultValue)
{
    ngsUnused(thisObj);
    return ngsJsonObjectGetIntegerForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), defaultValue);
}

NGS_JNI_FUNC(jlong, jsonObjectGetLongForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                             jstring name, jlong defaultValue)
{
    ngsUnused(thisObj);
    return ngsJsonObjectGetLongForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), defaultValue);
}

NGS_JNI_FUNC(jboolean, jsonObjectGetBoolForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                                jstring name, jboolean defaultValue)
{
    ngsUnused(thisObj);
    return ngsJsonObjectGetBoolForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), defaultValue) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetStringForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                                  jstring name, jstring value)
{
    ngsUnused(thisObj);
    return ngsJsonObjectSetStringForKey(reinterpret_cast<JsonObjectH>(object),
                                        jniString(env, name).c_str(),
                                        jniString(env, value).c_str()) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetDoubleForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                                  jstring name, jdouble value)
{
    ngsUnused(thisObj);
    return ngsJsonObjectSetDoubleForKey(reinterpret_cast<JsonObjectH>(object),
                                         jniString(env, name).c_str(), value) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetIntegerForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                                   jstring name, jint value)
{
    ngsUnused(thisObj);
    return ngsJsonObjectSetIntegerForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), value) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetLongForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                                jstring name, jlong value)
{
    ngsUnused(thisObj);
    return ngsJsonObjectSetLongForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), value) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, jsonObjectSetBoolForKey)(JNIEnv *env, jobject thisObj, jlong object,
                                                jstring name, jboolean value)
{
    ngsUnused(thisObj);
    return ngsJsonObjectSetBoolForKey(reinterpret_cast<JsonObjectH>(object),
                                      jniString(env, name).c_str(), value) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

/*
 * Catalog functions
 */

NGS_JNI_FUNC(jstring, catalogPathFromSystem)(JNIEnv *env, jobject thisObj, jstring path)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsCatalogPathFromSystem(jniString(env, path).c_str()));
}

NGS_JNI_FUNC(jlong, catalogObjectGet)(JNIEnv *env, jobject thisObj, jstring path)
{
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsCatalogObjectGet(jniString(env, path).c_str()));
}

NGS_JNI_FUNC(jlong, catalogObjectGetByName)(JNIEnv *env, jobject thisObj, jlong parent, jstring name,
        jboolean fullMatch)
{
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsCatalogObjectGetByName(
            reinterpret_cast<CatalogObjectH>(parent), jniString(env, name).c_str(),
            static_cast<char>(fullMatch ? 1 : 0)));
}

static jobjectArray catalogObjectQueryToJobjectArray(JNIEnv *env, ngsCatalogObjectInfo *info)
{
    if(nullptr == info) {
        return env->NewObjectArray(0, g_CatalogObjectInfoClass, nullptr);
    }

    std::vector<jobject> infoArray;
    int counter = 0;
    while(info[counter].name != nullptr) {
        jvalue args[3];
        args[0].l = env->NewStringUTF(info[counter].name);
        args[1].i = info[counter].type;
        args[2].j = reinterpret_cast<jlong>(info[counter].object);

        infoArray.push_back(env->NewObjectA(g_CatalogObjectInfoClass, g_CatalogObjectInfoInitMid, args));
        counter++;
    }

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(infoArray.size()), g_CatalogObjectInfoClass, nullptr);
    for(int i = 0; i < infoArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, infoArray[i]);
    }

    return array;
}

NGS_JNI_FUNC(jobjectArray, catalogObjectQuery)(JNIEnv *env, jobject thisObj, jlong object, jint filter)
{
    ngsUnused(thisObj);
    ngsCatalogObjectInfo *info = ngsCatalogObjectQuery(reinterpret_cast<CatalogObjectH>(object), filter);
    jobjectArray out = catalogObjectQueryToJobjectArray(env, info);

    if(nullptr != info) {
        ngsFree(info);
    }

    return out;
}

NGS_JNI_FUNC(jobjectArray, catalogObjectQueryMultiFilter)(JNIEnv *env, jobject thisObj, jlong object, jintArray filters)
{
    ngsUnused(thisObj);
    int size = env->GetArrayLength(filters);
    jboolean isCopy;
    jint *filtersArray = env->GetIntArrayElements(filters, &isCopy);
    ngsCatalogObjectInfo *info = ngsCatalogObjectQueryMultiFilter(
            reinterpret_cast<CatalogObjectH>(object), filtersArray, size);
    jobjectArray out = catalogObjectQueryToJobjectArray(env, info);
    if(nullptr != info) {
        ngsFree(info);
    }
    return out;
}

NGS_JNI_FUNC(jboolean, catalogObjectDelete)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsCatalogObjectDelete(reinterpret_cast<CatalogObjectH>(object)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, catalogObjectCreate)(JNIEnv *env, jobject thisObj, 
    jlong object, jstring name, jobjectArray options)
{
    ngsUnused(thisObj);
    char **nativeOptions = toOptions(env, options);
    jlong res = reinterpret_cast<jlong>(ngsCatalogObjectCreate(
        reinterpret_cast<CatalogObjectH>(object), jniString(env, name).c_str(), 
        nativeOptions));
    CSLDestroy(nativeOptions);
    return res;
}

NGS_JNI_FUNC(jboolean, catalogObjectCopy)(JNIEnv *env, jobject thisObj, jlong srcObject,
                                          jlong dstObjectContainer, jobjectArray options, jint callbackId)
{
    ngsUnused(thisObj);
    char **nativeOptions = toOptions(env, options);
    int result;
    if(callbackId == 0) {
        result = ngsCatalogObjectCopy(reinterpret_cast<CatalogObjectH>(srcObject),
                                      reinterpret_cast<CatalogObjectH>(dstObjectContainer),
                                      nativeOptions, nullptr, nullptr);
    }
    else {
        result = ngsCatalogObjectCopy(reinterpret_cast<CatalogObjectH>(srcObject),
                             reinterpret_cast<CatalogObjectH>(dstObjectContainer),
                             nativeOptions, progressProxyFunc,
                             reinterpret_cast<void *>(callbackId));
    }
    CSLDestroy(nativeOptions);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, catalogObjectRename)(JNIEnv *env, jobject thisObj, jlong object,
                                            jstring newName)
{
    ngsUnused(thisObj);
    return ngsCatalogObjectRename(reinterpret_cast<CatalogObjectH>(object),
                                  jniString(env, newName).c_str()) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jstring, catalogObjectOptions)(JNIEnv *env, jobject thisObj, jlong object, jint optionType)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsCatalogObjectOptions(reinterpret_cast<CatalogObjectH>(object), optionType));
}

NGS_JNI_FUNC(jint, catalogObjectType)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsCatalogObjectType(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(jstring, catalogObjectName)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsCatalogObjectName(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(jstring, catalogObjectPath)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsCatalogObjectPath(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(jobject, catalogObjectProperties)(JNIEnv *env, jobject thisObj, jlong object, jstring domain)
{
    ngsUnused(thisObj);
    char **properties = ngsCatalogObjectProperties(reinterpret_cast<CatalogObjectH>(object),
                                                   jniString(env, domain).c_str());
    jobject ret = fromOptions(env, reinterpret_cast<CSLConstList>(properties));
    ngsListFree(properties);
    return ret;
}

NGS_JNI_FUNC(jstring, catalogObjectGetProperty)(JNIEnv *env, jobject thisObj, jlong object,
                                                  jstring name, jstring defaultValue, jstring domain)
{
    ngsUnused(thisObj);
    const char *ret = ngsCatalogObjectProperty(reinterpret_cast<CatalogObjectH>(object),
                                          jniString(env, name).c_str(), jniString(env, defaultValue).c_str(),
                                          jniString(env, domain).c_str());
    return env->NewStringUTF(ret);
}

NGS_JNI_FUNC(jboolean , catalogObjectSetProperty)(JNIEnv *env, jobject thisObj, jlong object,
                                                  jstring name, jstring value, jstring domain)
{
    ngsUnused(thisObj);
    int ret = ngsCatalogObjectSetProperty(reinterpret_cast<CatalogObjectH>(object),
                                          jniString(env, name).c_str(), jniString(env, value).c_str(),
                                          jniString(env, domain).c_str());
    return ret == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(void, catalogObjectRefresh)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsCatalogObjectRefresh(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(jboolean, catalogCheckConnection)(JNIEnv *env, jobject thisObj, jint objectType, jobjectArray options)
{
    ngsUnused(thisObj);
    char **nativeOptions = toOptions(env, options);
    char res = ngsCatalogCheckConnection(static_cast<enum ngsCatalogObjectType>(objectType), nativeOptions);
    CSLDestroy(nativeOptions);
    return res == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

/*
 * Feature class
 */

NGS_JNI_FUNC(jboolean, datasetOpen)(JNIEnv *env, jobject thisObj, jlong object,
                                    jint openFlags, jobjectArray openOptions)
{
    ngsUnused(thisObj);
    char** openOptionsNative = toOptions(env, openOptions);
    int result = ngsDatasetOpen(reinterpret_cast<CatalogObjectH>(object),
                                static_cast<unsigned int>(openFlags), openOptionsNative);
    CSLDestroy(openOptionsNative);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, datasetIsOpened)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsDatasetIsOpened(reinterpret_cast<CatalogObjectH>(object)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, datasetClose)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsDatasetClose(reinterpret_cast<CatalogObjectH>(object)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobjectArray, featureClassFields)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    ngsField *fields = ngsFeatureClassFields(reinterpret_cast<CatalogObjectH>(object));
    if(nullptr == fields) {
        return env->NewObjectArray(0, g_CatalogObjectInfoClass, nullptr);
    }

    std::vector<jobject> fieldArray;
    int counter = 0;
    while(fields[counter].name != nullptr) {
        jvalue args[4];
        args[0].l = env->NewStringUTF(fields[counter].name);
        args[1].l = env->NewStringUTF(fields[counter].alias);
        args[2].i = fields[counter].type;
        args[3].l = nullptr;

        fieldArray.push_back(env->NewObjectA(g_FieldClass, g_FieldInitMid, args));
        counter++;
    }

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(fieldArray.size()), g_FieldClass, nullptr);
    for(int i = 0; i < fieldArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, fieldArray[i]);
    }

    return array;
}

NGS_JNI_FUNC(jint, featureClassGeometryType)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureClassGeometryType(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(jboolean, featureClassCreateOverviews)(JNIEnv *env, jobject thisObj, jlong object,
                                                    jobjectArray options, jint callbackId)
{
    ngsUnused(thisObj);
    char **nativeOptions = toOptions(env, options);
    int result;
    if(callbackId == 0) {
        result = ngsFeatureClassCreateOverviews(reinterpret_cast<CatalogObjectH>(object),
                                                nativeOptions, nullptr, nullptr);
    }
    else {
        result = ngsFeatureClassCreateOverviews(reinterpret_cast<CatalogObjectH>(object),
                                                nativeOptions, progressProxyFunc,
                                                reinterpret_cast<void *>(callbackId));
    }
    CSLDestroy(nativeOptions);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, featureClassCreateFeature)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsFeatureClassCreateFeature(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(void, featureClassBatchMode)(JNIEnv *env, jobject thisObj, jlong object, jboolean enable)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFeatureClassBatchMode(reinterpret_cast<CatalogObjectH>(object), enable);
}

NGS_JNI_FUNC(jboolean, featureClassInsertFeature)(JNIEnv *env, jobject thisObj, jlong object,
                                                  jlong feature, jboolean logEdits)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureClassInsertFeature(reinterpret_cast<CatalogObjectH>(object),
                                        reinterpret_cast<FeatureH>(feature), logEdits) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassUpdateFeature)(JNIEnv *env, jobject thisObj, jlong object, jlong feature, jboolean logEdits)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureClassUpdateFeature(reinterpret_cast<CatalogObjectH>(object),
                                        reinterpret_cast<FeatureH>(feature), logEdits) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassDeleteFeature)(JNIEnv *env, jobject thisObj, jlong object, jlong id, jboolean logEdits)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureClassDeleteFeature(reinterpret_cast<CatalogObjectH>(object), id, logEdits) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassDeleteFeatures)(JNIEnv *env, jobject thisObj, jlong object, jboolean logEdits)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureClassDeleteFeatures(reinterpret_cast<CatalogObjectH>(object), logEdits) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, featureClassCount)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureClassCount(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(void, featureClassResetReading)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFeatureClassResetReading(reinterpret_cast<CatalogObjectH>(object));
}

NGS_JNI_FUNC(jlong, featureClassNextFeature)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsFeatureClassNextFeature(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(jlong, featureClassGetFeature)(JNIEnv *env, jobject thisObj, jlong object, jlong id)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsFeatureClassGetFeature(reinterpret_cast<CatalogObjectH>(object), id));
}

NGS_JNI_FUNC(jboolean, featureClassSetFilter)(JNIEnv *env, jobject thisObj, jlong object,
                                              jlong geometryFilter, jstring attributeFilter)
{
    ngsUnused(thisObj);
    return ngsFeatureClassSetFilter(reinterpret_cast<CatalogObjectH>(object),
                                    reinterpret_cast<GeometryH>(geometryFilter),
                                    jniString(env, attributeFilter).c_str()) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassSetSpatialFilter)(JNIEnv *env, jobject thisObj, jlong object,
                                                        jdouble minX, jdouble minY, jdouble maxX, jdouble maxY)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureClassSetSpatialFilter(reinterpret_cast<CatalogObjectH>(object),
                                           minX, minY, maxX, maxY) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureClassDeleteEditOperation)(JNIEnv *env, jobject thisObj, jlong object,
                                                        jlong fid, jlong aid, jint code, jlong rid,
                                                        jlong arid)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsEditOperation operation = {fid, aid, static_cast<ngsChangeCode>(code), rid, arid};
    return ngsFeatureClassDeleteEditOperation(reinterpret_cast<CatalogObjectH>(object),
                                              operation) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobjectArray, featureClassGetEditOperations)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    ngsEditOperation *out = ngsFeatureClassGetEditOperations(
            reinterpret_cast<CatalogObjectH>(object));
    int counter = 0;
    std::vector<jobject> obArray;
    while(out[counter].fid != -1) {
        jvalue args[5];
        args[0].j = out[counter].fid;
        args[1].j = out[counter].aid;
        args[2].i = out[counter].code;
        args[3].j = out[counter].rid;
        args[4].j = out[counter].arid;

        obArray.push_back(env->NewObjectA(g_EditOperationClass, g_EditOperationInitMid, args));
        counter++;
    }
    ngsFree(out);

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(obArray.size()), g_EditOperationClass, nullptr);
    for(int i = 0; i < obArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, obArray[i]);
    }

    return array;
}

NGS_JNI_FUNC(void, featureFree)(JNIEnv *env, jobject thisObj, jlong feature)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFeatureFree(reinterpret_cast<FeatureH>(feature));
}

NGS_JNI_FUNC(jint, featureFieldCount)(JNIEnv *env, jobject thisObj, jlong feature)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureFieldCount(reinterpret_cast<FeatureH>(feature));
}

NGS_JNI_FUNC(jboolean, featureIsFieldSet)(JNIEnv *env, jobject thisObj, jlong feature, jint fieldIndex)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureIsFieldSet(reinterpret_cast<FeatureH>(feature), fieldIndex) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, featureGetId)(JNIEnv *env, jobject thisObj, jlong feature)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureGetId(reinterpret_cast<FeatureH>(feature));
}

NGS_JNI_FUNC(jlong, featureGetGeometry)(JNIEnv *env, jobject thisObj, jlong feature)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsFeatureGetGeometry(reinterpret_cast<FeatureH>(feature)));
}

NGS_JNI_FUNC(jint, featureGetFieldAsInteger)(JNIEnv *env, jobject thisObj, jlong feature, jint field)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureGetFieldAsInteger(reinterpret_cast<FeatureH>(feature), field);
}

NGS_JNI_FUNC(jdouble, featureGetFieldAsDouble)(JNIEnv *env, jobject thisObj, jlong feature, jint field)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureGetFieldAsDouble(reinterpret_cast<FeatureH>(feature), field);
}

NGS_JNI_FUNC(jstring, featureGetFieldAsString)(JNIEnv *env, jobject thisObj, jlong feature, jint field)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsFeatureGetFieldAsString(reinterpret_cast<FeatureH>(feature), field));
}

NGS_JNI_FUNC(jobject, featureGetFieldAsDateTime)(JNIEnv *env, jobject thisObj, jlong feature, jint field)
{
    ngsUnused(thisObj);
    int year(0), month(0), day(0), hour(0), minute(0), tzflag(0);
    float second(0.0f);
    ngsFeatureGetFieldAsDateTime(reinterpret_cast<FeatureH>(feature), field, &year, &month, &day, &hour, &minute, &second, &tzflag);
    if( tzflag > 1 ) {
        tzflag = (tzflag - 100) / 4 * 3600000; // milliseconds
    }
    else {
        tzflag = 0; // UTC
    }
    jvalue args[7];
    args[0].i = year;
    args[1].i = month;
    args[2].i = day;
    args[3].i = hour;
    args[4].i = minute;
    args[5].i = static_cast<jint>(second);
    args[6].i = tzflag;
    return env->NewObjectA(g_DateComponentsClass, g_DateComponentsInitMid, args);
}

NGS_JNI_FUNC(void, featureSetGeometry)(JNIEnv *env, jobject thisObj, jlong feature, jlong geometry)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFeatureSetGeometry(reinterpret_cast<FeatureH>(feature), reinterpret_cast<GeometryH>(geometry));
}

NGS_JNI_FUNC(void, featureSetFieldInteger)(JNIEnv *env, jobject thisObj, jlong feature, jint field, jint value)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFeatureSetFieldInteger(reinterpret_cast<FeatureH>(feature), field, value);
}

NGS_JNI_FUNC(void, featureSetFieldDouble)(JNIEnv *env, jobject thisObj, jlong feature, jint field, jdouble value)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFeatureSetFieldDouble(reinterpret_cast<FeatureH>(feature), field, value);
}

NGS_JNI_FUNC(void, featureSetFieldString)(JNIEnv *env, jobject thisObj, jlong feature, jint field, jstring value)
{
    ngsUnused(thisObj);
    ngsFeatureSetFieldString(reinterpret_cast<FeatureH>(feature), field, jniString(env, value).c_str());
}

NGS_JNI_FUNC(void, featureSetFieldDateTime)(JNIEnv *env, jobject thisObj, jlong feature, jint field,
                                            jint year, jint month, jint day, jint hour, jint minute,
                                            jint second)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsFeatureSetFieldDateTime(reinterpret_cast<FeatureH>(feature), field, year, month, day, hour, minute, second, 100); // 100 is UTC
}

NGS_JNI_FUNC(jlong, storeFeatureClassGetFeatureByRemoteId)(JNIEnv *env, jobject thisObj, jlong object, jlong rid)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsStoreFeatureClassGetFeatureByRemoteId(
            reinterpret_cast<CatalogObjectH>(object), rid));
}

NGS_JNI_FUNC(jlong, storeFeatureGetRemoteId)(JNIEnv *env, jobject thisObj, jlong feature)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsStoreFeatureGetRemoteId(reinterpret_cast<FeatureH>(feature));
}

NGS_JNI_FUNC(void, storeFeatureSetRemoteId)(JNIEnv *env, jobject thisObj, jlong feature, jlong rid)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsStoreFeatureSetRemoteId(reinterpret_cast<FeatureH>(feature), rid);
}

NGS_JNI_FUNC(jlong, featureCreateGeometry)(JNIEnv *env, jobject thisObj, jlong feature)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsFeatureCreateGeometry(reinterpret_cast<FeatureH>(feature)));
}

NGS_JNI_FUNC(jlong, featureCreateGeometryFromJson)(JNIEnv *env, jobject thisObj, jlong geometry)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsFeatureCreateGeometryFromJson(reinterpret_cast<JsonObjectH>(geometry)));
}

NGS_JNI_FUNC(void, geometryFree)(JNIEnv *env, jobject thisObj, jlong geometry)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsGeometryFree(reinterpret_cast<GeometryH>(geometry));
}

NGS_JNI_FUNC(void, geometrySetPoint)(JNIEnv *env, jobject thisObj, jlong geometry, jint point,
                                     jdouble x, jdouble y, jdouble z, jdouble m)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsGeometrySetPoint(reinterpret_cast<GeometryH>(geometry), point, x, y, z, m);
}

static jobject toEnvelope(JNIEnv *env, const ngsExtent &ext)
{
    jvalue args[4];
    args[0].d = ext.minX;
    args[1].d = ext.maxX;
    args[2].d = ext.minY;
    args[3].d = ext.maxY;
    return env->NewObjectA(g_EnvelopeClass, g_EnvelopeInitMid, args);
}

NGS_JNI_FUNC(jobject, geometryGetEnvelope)(JNIEnv *env, jobject thisObj, jlong geometry)
{
    ngsUnused(thisObj);
    ngsExtent ext = ngsGeometryGetEnvelope(reinterpret_cast<GeometryH>(geometry));
    return toEnvelope(env, ext);
}

NGS_JNI_FUNC(jboolean, geometryTransformTo)(JNIEnv *env, jobject thisObj, jlong geometry, jint  EPSG)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsGeometryTransformTo(reinterpret_cast<GeometryH>(geometry), EPSG) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, geometryTransform)(JNIEnv *env, jobject thisObj, jlong geometry, jlong ct)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsGeometryTransform(reinterpret_cast<GeometryH>(geometry),
                                reinterpret_cast<CoordinateTransformationH>(ct)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, geometryIsEmpty)(JNIEnv *env, jobject thisObj, jlong geometry)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsGeometryIsEmpty(reinterpret_cast<GeometryH>(geometry)) == 0 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jint, geometryGetType)(JNIEnv *env, jobject thisObj, jlong geometry)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsGeometryGetType(reinterpret_cast<GeometryH>(geometry));
}

NGS_JNI_FUNC(jstring, geometryToJson)(JNIEnv *env, jobject thisObj, jlong geometry)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsGeometryToJson(reinterpret_cast<GeometryH>(geometry)));
}

NGS_JNI_FUNC(jlong, coordinateTransformationCreate)(JNIEnv *env, jobject thisObj,
                                                    jint fromEPSG, jint toEPSG)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsCoordinateTransformationCreate(fromEPSG, toEPSG));
}

NGS_JNI_FUNC(void, coordinateTransformationFree)(JNIEnv *env, jobject thisObj, jlong handle)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsCoordinateTransformationFree(reinterpret_cast<CoordinateTransformationH>(handle));
}

NGS_JNI_FUNC(jobject, coordinateTransformationDo)(JNIEnv *env, jobject thisObj, jlong object,
                                                  jdouble x, jdouble y)
{
    ngsUnused(thisObj);
    ngsCoordinate coord = {x, y, 0.0};
    coord = ngsCoordinateTransformationDo(reinterpret_cast<CoordinateTransformationH>(object), coord);
    jvalue args[2];
    args[0].d = coord.X;
    args[1].d = coord.Y;
    return  env->NewObjectA(g_PointClass, g_PointInitMid, args);
}

NGS_JNI_FUNC(jlong, featureAttachmentAdd)(JNIEnv *env, jobject thisObj,
                                          jlong feature, jstring name, jstring description,
                                          jstring path, jobjectArray options, jboolean logEdits)
{
    ngsUnused(thisObj);
    return ngsFeatureAttachmentAdd(reinterpret_cast<FeatureH>(feature), jniString(env, name).c_str(),
        jniString(env, description).c_str(), jniString(env, path).c_str(), toOptions(env, options),
                                   logEdits);
}

NGS_JNI_FUNC(jboolean, featureAttachmentDelete)(JNIEnv *env, jobject thisObj,
                                                jlong feature, jlong aid, jboolean logEdits)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureAttachmentDelete(reinterpret_cast<FeatureH>(feature), aid, logEdits) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, featureAttachmentDeleteAll)(JNIEnv *env, jobject thisObj,
                                                   jlong feature, jboolean logEdits)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsFeatureAttachmentDeleteAll(reinterpret_cast<FeatureH>(feature), logEdits) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobject, featureAttachmentsGet)(JNIEnv *env, jobject thisObj, jlong feature)
{
    ngsUnused(thisObj);
    ngsFeatureAttachmentInfo *out = ngsFeatureAttachmentsGet(reinterpret_cast<FeatureH>(feature));
    int counter = 0;
    std::vector<jobject> obArray;
    while(out[counter].id != -1) {
        jvalue args[7];
        args[0].j = feature;
        args[1].j = out->id;
        args[2].l = env->NewStringUTF(out->name);
        args[3].l = env->NewStringUTF(out->description);
        args[4].l = env->NewStringUTF(out->path);
        args[5].j = out->size;
        args[6].j = out->rid;
        obArray.push_back(env->NewObjectA(g_EditOperationClass, g_EditOperationInitMid, args));
        counter++;
    }

    ngsFree(out);

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(obArray.size()), g_AttachmentClass, nullptr);
    for(int i = 0; i < obArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, obArray[i]);
    }

    return array;
}

NGS_JNI_FUNC(jboolean, featureAttachmentUpdate)(JNIEnv *env, jobject thisObj, jlong feature,
                                                jlong aid, jstring name, jstring description, jboolean logEdits)
{
    ngsUnused(thisObj);
    return ngsFeatureAttachmentUpdate(reinterpret_cast<FeatureH>(feature),
                                      aid, jniString(env, name).c_str(),
                                      jniString(env, description).c_str(), logEdits) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(void, storeFeatureSetAttachmentRemoteId)(JNIEnv *env, jobject thisObj,
                                                      jlong feature, jlong aid, jlong rid)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsStoreFeatureSetAttachmentRemoteId(reinterpret_cast<FeatureH>(feature), aid, rid);
}

/*
 * Raster
 */

NGS_JNI_FUNC(jboolean, rasterCacheArea)(JNIEnv *env, jobject thisObj, jlong object, jobjectArray options, jint callbackId)
{
    ngsUnused(thisObj);
    char **nativeOptions = toOptions(env, options);
    int result;
    if(callbackId == 0) {
        result = ngsRasterCacheArea(reinterpret_cast<CatalogObjectH>(object),
                                    nativeOptions, nullptr, nullptr);
    }
    else {
        result = ngsRasterCacheArea(reinterpret_cast<CatalogObjectH>(object),
                                    nativeOptions, progressProxyFunc,
                                    reinterpret_cast<void *>(callbackId));
    }
    CSLDestroy(nativeOptions);
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jint, mapCreate)(JNIEnv *env, jobject thisObj, jstring name, jstring description,
    jint epsg, jdouble minX, jdouble minY, jdouble maxX, jdouble maxY)
{
    ngsUnused(thisObj);
    return static_cast<jint>(ngsMapCreate(jniString(env, name).c_str(),
                                          jniString(env, description).c_str(),
                                          static_cast<unsigned short>(epsg), minX, minY, maxX, maxY));
}

NGS_JNI_FUNC(jint, mapOpen)(JNIEnv *env, jobject thisObj, jstring path)
{
    ngsUnused(thisObj);
    return static_cast<jint>(ngsMapOpen(jniString(env, path).c_str()));
}

NGS_JNI_FUNC(jboolean, mapSave)(JNIEnv *env, jobject thisObj, jint mapId, jstring path)
{
    ngsUnused(thisObj);
    return ngsMapSave(static_cast<char>(mapId), jniString(env, path).c_str()) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapClose)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapClose(static_cast<char>(mapId)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jint, mapReopen)(JNIEnv *env, jobject thisObj, jint mapId, jstring path)
{
    ngsUnused(thisObj);
    return static_cast<jint>(ngsMapReopen(static_cast<char>(mapId), jniString(env, path).c_str()));
}

NGS_JNI_FUNC(jint, mapLayerCount)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapLayerCount(static_cast<char>(mapId));
}

NGS_JNI_FUNC(jint, mapCreateLayer)(JNIEnv *env, jobject thisObj, jint mapId, jstring name, jstring path)
{
    ngsUnused(thisObj);
    return ngsMapCreateLayer(static_cast<char>(mapId), jniString(env, name).c_str(),
                             jniString(env, path).c_str());
}

NGS_JNI_FUNC(jlong, mapLayerGet)(JNIEnv *env, jobject thisObj, jint mapId, jint layerId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsMapLayerGet(static_cast<char>(mapId), layerId));
}

NGS_JNI_FUNC(jboolean, mapLayerDelete)(JNIEnv *env, jobject thisObj, jint mapId, jlong layer)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapLayerDelete(static_cast<char>(mapId), reinterpret_cast<LayerH>(layer)) ==
           COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapLayerReorder)(JNIEnv *env, jobject thisObj, jint mapId, jlong beforeLayer, jlong movedLayer)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapLayerReorder(static_cast<char>(mapId),
                              reinterpret_cast<LayerH>(beforeLayer),
                              reinterpret_cast<LayerH>(movedLayer)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapSetSize)(JNIEnv *env, jobject thisObj, jint mapId, jint width, jint height,
                                   jboolean YAxisInverted)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapSetSize(static_cast<char>(mapId), width, height, static_cast<char>(YAxisInverted ? 1 : 0)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapDraw)(JNIEnv *env, jobject thisObj, jint mapId, jint state, jint callbackId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    int result;
    if(callbackId == 0) {
        result = ngsMapDraw(static_cast<char>(mapId), static_cast<ngsDrawState>(state),
                nullptr, nullptr);
    }
    else {
        result = ngsMapDraw(static_cast<char>(mapId), static_cast<ngsDrawState>(state),
                progressProxyFunc, reinterpret_cast<void *>(callbackId));
    }
    return result == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapInvalidate)(JNIEnv *env, jobject thisObj, jint mapId,
                                      jdouble minX, jdouble minY, jdouble maxX, jdouble maxY)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapInvalidate(static_cast<char>(mapId), {minX, minY, maxX, maxY}) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapSetBackgroundColor)(JNIEnv *env, jobject thisObj, jint mapId, jint R, jint G, jint B, jint A)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapSetBackgroundColor(static_cast<char>(mapId), {
            static_cast<unsigned char>(R), static_cast<unsigned char>(G),
            static_cast<unsigned char>(B), static_cast<unsigned char>(A)}) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobject, mapGetBackgroundColor)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(thisObj);
    ngsRGBA rgba = ngsMapGetBackgroundColor(static_cast<char>(mapId));
    jvalue args[4];
    args[0].i = rgba.R;
    args[1].i = rgba.G;
    args[2].i = rgba.B;
    args[3].i = rgba.A;
    return env->NewObjectA(g_RGBAClass, g_RGBAInitMid, args);
}

NGS_JNI_FUNC(jboolean, mapSetCenter)(JNIEnv *env, jobject thisObj, jint mapId, jdouble x, jdouble y)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapSetCenter(static_cast<char>(mapId), x, y) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobject, mapGetCenter)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(thisObj);
    ngsCoordinate coord = ngsMapGetCenter(static_cast<char>(mapId));
    jvalue args[2];
    args[0].d = coord.X;
    args[1].d = coord.Y;
    return  env->NewObjectA(g_PointClass, g_PointInitMid, args);
}

NGS_JNI_FUNC(jobject, mapGetCoordinate)(JNIEnv *env, jobject thisObj, jint mapId, jdouble x, jdouble y)
{
    ngsUnused(thisObj);
    ngsCoordinate coord = ngsMapGetCoordinate(static_cast<char>(mapId), x, y);
    jvalue args[2];
    args[0].d = coord.X;
    args[1].d = coord.Y;
    return  env->NewObjectA(g_PointClass, g_PointInitMid, args);
}

NGS_JNI_FUNC(jobject, mapGetDistance)(JNIEnv *env, jobject thisObj, jint mapId, jdouble w, jdouble h)
{
    ngsUnused(thisObj);
    ngsCoordinate coord = ngsMapGetDistance(static_cast<char>(mapId), w, h);
    jvalue args[2];
    args[0].d = coord.X;
    args[1].d = coord.Y;
    return  env->NewObjectA(g_PointClass, g_PointInitMid, args);
}

NGS_JNI_FUNC(jboolean, mapSetRotate)(JNIEnv *env, jobject thisObj, jint mapId, jint direction, jdouble rotate)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapSetRotate(static_cast<char>(mapId), static_cast<ngsDirection>(direction),
                           rotate) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jdouble, mapGetRotate)(JNIEnv *env, jobject thisObj, jint mapId, jint direction)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapGetRotate(static_cast<char>(mapId), static_cast<ngsDirection>(direction));
}

NGS_JNI_FUNC(jboolean, mapSetScale)(JNIEnv *env, jobject thisObj, jint mapId, jdouble scale)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapSetScale(static_cast<char>(mapId), scale) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jdouble, mapGetScale)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapGetScale(static_cast<char>(mapId));
}

NGS_JNI_FUNC(jboolean, mapSetOptions)(JNIEnv *env, jobject thisObj, jint mapId, jobjectArray options)
{
    ngsUnused(thisObj);
    char **thisOptions = toOptions(env, options);
    int ret = ngsMapSetOptions(static_cast<char>(mapId), thisOptions);
    ngsFree(thisOptions);
    return ret == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapSetExtentLimits)(JNIEnv *env, jobject thisObj, jint mapId,
                                           jdouble minX, jdouble minY, jdouble maxX, jdouble maxY)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapSetExtentLimits(static_cast<char>(mapId), minX, minY, maxX, maxY) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobject, mapGetExtent)(JNIEnv *env, jobject thisObj, jint mapId, jint epsg)
{
    ngsUnused(thisObj);
    ngsExtent ext = ngsMapGetExtent(static_cast<char>(mapId), epsg);
    return toEnvelope(env, ext);
}

NGS_JNI_FUNC(jboolean, mapSetExtent)(JNIEnv *env, jobject thisObj, jint mapId,
                                     jdouble minX, jdouble minY, jdouble maxX, jdouble maxY)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapSetExtent(static_cast<char>(mapId), {minX, minY, maxX, maxY}) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, mapGetSelectionStyle)(JNIEnv *env, jobject thisObj, jint mapId, jint styleType)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsMapGetSelectionStyle(static_cast<char>(mapId),
                                                           static_cast<ngsStyleType>(styleType)));
}

NGS_JNI_FUNC(jboolean, mapSetSelectionsStyle)(JNIEnv *env, jobject thisObj, jint mapId, jint styleType, jlong style)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsMapSetSelectionsStyle(static_cast<char>(mapId),
                                    static_cast<ngsStyleType>(styleType),
                                    reinterpret_cast<JsonObjectH>(style)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jstring, mapGetSelectionStyleName)(JNIEnv *env, jobject thisObj, jint mapId, jint styleType)
{
    ngsUnused(thisObj);
    const char *styleName = ngsMapGetSelectionStyleName(static_cast<char>(mapId),
                                                        static_cast<ngsStyleType>(styleType));
    return env->NewStringUTF(styleName);
}

NGS_JNI_FUNC(jboolean, mapSetSelectionStyleName)(JNIEnv *env, jobject thisObj, jint mapId,
                                                 jint styleType, jstring name)
{
    ngsUnused(thisObj);
    return ngsMapSetSelectionStyleName(static_cast<char>(mapId),
                                       static_cast<ngsStyleType>(styleType),
                                       jniString(env, name).c_str()) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapIconSetAdd)(JNIEnv *env, jobject thisObj, jint mapId, jstring name, jstring path, jboolean ownByMap)
{
    ngsUnused(thisObj);
    return ngsMapIconSetAdd(static_cast<char>(mapId), jniString(env, name).c_str(),
                            jniString(env, path).c_str(), static_cast<char>(ownByMap ? 1 : 0)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapIconSetRemove)(JNIEnv *env, jobject thisObj, jint mapId, jstring name)
{
    ngsUnused(thisObj);
    return ngsMapIconSetRemove(static_cast<char>(mapId), jniString(env, name).c_str()) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, mapIconSetExists)(JNIEnv *env, jobject thisObj, jint mapId, jstring name)
{
    ngsUnused(thisObj);
    return ngsMapIconSetExists(static_cast<char>(mapId), jniString(env, name).c_str()) == 1 ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

/*
 * Layer functions
 */

NGS_JNI_FUNC(jstring, layerGetName)(JNIEnv *env, jobject thisObj, jlong layer)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsLayerGetName(reinterpret_cast<LayerH>(layer)));
}

NGS_JNI_FUNC(jboolean, layerSetName)(JNIEnv *env, jobject thisObj, jlong layer, jstring name)
{
    ngsUnused(thisObj);
    return ngsLayerSetName(reinterpret_cast<LayerH>(layer),
                           jniString(env, name).c_str()) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, layerGetVisible)(JNIEnv *env, jobject thisObj, jlong layer)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLayerGetVisible(reinterpret_cast<LayerH>(layer)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jfloat, layerGetMaxZoom)(JNIEnv *env, jobject thisObj, jlong layer)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLayerGetMaxZoom(reinterpret_cast<LayerH>(layer));
}

NGS_JNI_FUNC(jfloat, layerGetMinZoom)(JNIEnv *env, jobject thisObj, jlong layer)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLayerGetMinZoom(reinterpret_cast<LayerH>(layer));
}

NGS_JNI_FUNC(jboolean, layerSetVisible)(JNIEnv *env, jobject thisObj, jlong layer, jboolean visible)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLayerSetVisible(reinterpret_cast<LayerH>(layer), static_cast<char>(visible ? 1 : 0)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, layerSetMaxZoom)(JNIEnv *env, jobject thisObj, jlong layer, jfloat zoom)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLayerSetMaxZoom(reinterpret_cast<LayerH>(layer), zoom) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, layerSetMinZoom)(JNIEnv *env, jobject thisObj, jlong layer, jfloat zoom)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLayerSetMinZoom(reinterpret_cast<LayerH>(layer), zoom) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, layerGetDataSource)(JNIEnv *env, jobject thisObj, jlong layer)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsLayerGetDataSource(reinterpret_cast<LayerH>(layer)));
}

NGS_JNI_FUNC(jlong, layerGetStyle)(JNIEnv *env, jobject thisObj, jlong layer)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsLayerGetStyle(reinterpret_cast<LayerH>(layer)));
}

NGS_JNI_FUNC(jboolean, layerSetStyle)(JNIEnv *env, jobject thisObj, jlong layer, jlong style)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLayerSetStyle(reinterpret_cast<LayerH>(layer),
                            reinterpret_cast<JsonObjectH>(style)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jstring, layerGetStyleName)(JNIEnv *env, jobject thisObj, jlong layer)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsLayerGetStyleName(reinterpret_cast<LayerH>(layer)));
}

NGS_JNI_FUNC(jboolean, layerSetStyleName)(JNIEnv *env, jobject thisObj, jlong layer, jstring name)
{
    ngsUnused(thisObj);
    return ngsLayerSetStyleName(reinterpret_cast<LayerH>(layer), jniString(env, name).c_str()) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, layerSetSelectionIds)(JNIEnv *env, jobject thisObj, jlong layer, jlongArray ids)
{
    ngsUnused(thisObj);
    int size = env->GetArrayLength(ids);
    jboolean isCopy;
    jlong *idsArray = env->GetLongArrayElements(ids, &isCopy);
    std::vector<long long> idsStdArray;
    for(int i = 0; i < size; ++i) {
        idsStdArray.push_back(idsArray[i]);
    }
    return ngsLayerSetSelectionIds(reinterpret_cast<LayerH>(layer),
                                   idsStdArray.data(), size) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, layerSetHideIds)(JNIEnv *env, jobject thisObj, jlong layer, jlongArray ids)
{
    ngsUnused(thisObj);
    int size = env->GetArrayLength(ids);
    jboolean isCopy;
    jlong *idsArray = env->GetLongArrayElements(ids, &isCopy);
    std::vector<long long> idsStdArray;
    for(int i = 0; i < size; ++i) {
        idsStdArray.push_back(idsArray[i]);
    }
    return ngsLayerSetHideIds(reinterpret_cast<LayerH>(layer),
                              idsStdArray.data(), size) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

/*
 * Overlay functions
 */

NGS_JNI_FUNC(jboolean, overlaySetVisible)(JNIEnv *env, jobject thisObj, jint mapId, jint typeMask,
                                          jboolean visible)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsOverlaySetVisible(static_cast<char>(mapId), typeMask,
                                visible ? 1 : 0) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, overlayGetVisible)(JNIEnv *env, jobject thisObj, jint mapId, jint type)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsOverlayGetVisible(static_cast<char>(mapId),
                                static_cast<ngsMapOverlayType>(type)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, overlaySetOptions)(JNIEnv *env, jobject thisObj, jint mapId, jint type, jobjectArray options)
{
    ngsUnused(thisObj);
    char **thisOptions = toOptions(env, options);
    int ret = ngsOverlaySetOptions(static_cast<char>(mapId),
                                   static_cast<ngsMapOverlayType>(type), thisOptions);
    ngsFree(thisOptions);
    return ret == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jobjectArray, overlayGetOptions)(JNIEnv *env, jobject thisObj, jint mapId, jint type)
{
    ngsUnused(thisObj);
    char **outOptions = ngsOverlayGetOptions(static_cast<char>(mapId),
                                             static_cast<ngsMapOverlayType>(type));
    jobjectArray ret = fromOptions(env, outOptions);
    ngsListFree(outOptions);
    return ret;
}

/* Edit */

NGS_JNI_FUNC(jobject, editOverlayTouch)(JNIEnv *env, jobject thisObj, jint mapId, jdouble x, jdouble y, jint type)
{
    ngsUnused(thisObj);
    ngsPointId pointId = ngsEditOverlayTouch(static_cast<char>(mapId), x, y,
                                             static_cast<ngsMapTouchType>(type));
    jvalue args[2];
    args[0].i = pointId.pointId;
    args[1].z = pointId.isHole == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
    return env->NewObjectA(g_TouchResultClass, g_TouchResultInitMid, args);
}

NGS_JNI_FUNC(jboolean, editOverlayUndo)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayUndo(static_cast<char>(mapId)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, editOverlayRedo)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayRedo(static_cast<char>(mapId)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, eitOverlayCanUndo)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayCanUndo(static_cast<char>(mapId)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, editOverlayCanRedo)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayCanRedo(static_cast<char>(mapId)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, editOverlaySave)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsEditOverlaySave(static_cast<char>(mapId)));
}

NGS_JNI_FUNC(jboolean, editOverlayCancel)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayCancel(static_cast<char>(mapId)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, editOverlayCreateGeometryInLayer)(JNIEnv *env, jobject thisObj, jint mapId, jlong layer, jboolean empty)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayCreateGeometryInLayer(static_cast<char>(mapId),
                                               reinterpret_cast<LayerH>(layer), static_cast<char>(empty ? 1 : 0)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, editOverlayCreateGeometry)(JNIEnv *env, jobject thisObj, jint mapId, jint type)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayCreateGeometry(static_cast<char>(mapId),
                                        static_cast<ngsGeometryType>(type)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, editOverlayEditGeometry)(JNIEnv *env, jobject thisObj, jint mapId, jlong layer, jlong featureId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayEditGeometry(static_cast<char>(mapId),
                                      reinterpret_cast<LayerH>(layer), featureId) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;

}

NGS_JNI_FUNC(jboolean, editOverlayDeleteGeometry)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayDeleteGeometry(static_cast<char>(mapId)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, editOverlayAddPoint)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayAddPoint(static_cast<char>(mapId)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, editOverlayAddVertex)(JNIEnv *env, jobject thisObj, jint mapId,
                                             jdouble x, jdouble y, jdouble z)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayAddVertex(static_cast<char>(mapId), {x, y, z}) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jint, editOverlayDeletePoint)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayDeletePoint(static_cast<char>(mapId));
}

NGS_JNI_FUNC(jboolean, editOverlayAddHole)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return static_cast<jboolean>(ngsEditOverlayAddHole(static_cast<char>(mapId)));
}

NGS_JNI_FUNC(jint, editOverlayDeleteHole)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayDeleteHole(static_cast<char>(mapId));
}

NGS_JNI_FUNC(jboolean, editOverlayAddGeometryPart)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayAddGeometryPart(static_cast<char>(mapId)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jint, editOverlayDeleteGeometryPart)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayDeleteGeometryPart(static_cast<char>(mapId));
}

NGS_JNI_FUNC(jlong, editOverlayGetGeometry)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsEditOverlayGetGeometry(static_cast<char>(mapId)));
}

NGS_JNI_FUNC(jboolean, editOverlaySetStyle)(JNIEnv *env, jobject thisObj, jint mapId, jint type, jlong style)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlaySetStyle(static_cast<char>(mapId),
                                  static_cast<ngsEditStyleType>(type),
                                  reinterpret_cast<JsonObjectH>(style)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, editOverlaySetStyleName)(JNIEnv *env, jobject thisObj, jint mapId, jint type, jstring name)
{
    ngsUnused(thisObj);
    return ngsEditOverlaySetStyleName(static_cast<char>(mapId),
                                      static_cast<ngsEditStyleType>(type),
                                      jniString(env, name).c_str()) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, editOverlayGetStyl)(JNIEnv *env, jobject thisObj, jint mapId, jint type)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsEditOverlayGetStyle(static_cast<char>(mapId),
                                                          static_cast<ngsEditStyleType>(type)));
}

NGS_JNI_FUNC(void, editOverlaySetWalkingMode)(JNIEnv *env, jobject thisObj, jint mapId, jboolean enable)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    ngsEditOverlaySetWalkingMode(static_cast<char>(mapId), static_cast<char>(enable ? 1 : 0));
}

NGS_JNI_FUNC(jboolean, editOverlayGetWalkingMode)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsEditOverlayGetWalkingMode(static_cast<char>(mapId)) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

/* Location */

NGS_JNI_FUNC(jboolean, locationOverlayUpdate)(JNIEnv *env, jobject thisObj, jint mapId,
                                              jdouble x, jdouble y, jdouble z, jdouble direction,
                                              jdouble accuracy)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLocationOverlayUpdate(static_cast<char>(mapId), {x, y, z},
                                    static_cast<float>(direction),
                                    static_cast<float>(accuracy)) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, locationOverlaySetStyle)(JNIEnv *env, jobject thisObj, jint mapId, jlong style)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsLocationOverlaySetStyle(static_cast<char>(mapId),
                                      reinterpret_cast<JsonObjectH>(style)) == COD_SUCCESS ?
           NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, locationOverlaySetStyleName)(JNIEnv *env, jobject thisObj, jint mapId, jstring name)
{
    ngsUnused(thisObj);
    return ngsLocationOverlaySetStyleName(static_cast<char>(mapId),
                                          jniString(env, name).c_str()) == COD_SUCCESS ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, locationOverlayGetStyle)(JNIEnv *env, jobject thisObj, jint mapId)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return reinterpret_cast<jlong>(ngsLocationOverlayGetStyle(static_cast<char>(mapId)));
}

/*
 * QMS
 */

NGS_JNI_FUNC(jobjectArray, QMSQuery)(JNIEnv *env, jobject thisObj, jobjectArray options)
{
    ngsUnused(thisObj);
    char **thisOptions = toOptions(env, options);
    ngsQMSItem *result = ngsQMSQuery(thisOptions);
    ngsFree(thisOptions);
    int counter = 0;
    std::vector<jobject> obArray;
    while(result[counter].id != -1) {
        jvalue args[8];
        args[0].i = result[counter].id;
        args[1].l = env->NewStringUTF(result[counter].name);
        args[2].l = env->NewStringUTF(result[counter].desc);
        args[3].i = result[counter].type;
        args[4].l = env->NewStringUTF(result[counter].iconUrl);
        args[5].i = result[counter].status;
        args[6].l = toEnvelope(env, result[counter].extent);
        args[7].i = result[counter].total;
        obArray.push_back(env->NewObjectA(g_QMSItemClass, g_QMSItemInitMid, args));
        counter++;
    }
    ngsFree(result);

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(obArray.size()), g_QMSItemClass, nullptr);
    for(int i = 0; i < obArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, obArray[i]);
    }
    return array;
}

NGS_JNI_FUNC(jobject, QMSQueryProperties)(JNIEnv *env, jobject thisObj, jint itemId)
{
    ngsUnused(thisObj);
    ngsQMSItemProperties properties = ngsQMSQueryProperties(itemId);
    jvalue args[12];
    args[0].i = properties.id;
    args[1].i = properties.status;
    args[2].l = env->NewStringUTF(properties.url);
    args[3].l = env->NewStringUTF(properties.name);
    args[4].l = env->NewStringUTF(properties.desc);
    args[5].i = properties.type;
    args[6].i = properties.EPSG;
    args[7].i = properties.z_min;
    args[8].i = properties.z_max;
    args[9].l = env->NewStringUTF(properties.iconUrl);
    args[10].l = toEnvelope(env, properties.extent);
    args[11].z = properties.y_origin_top == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
    return env->NewObjectA(g_QMSItemPropertiesClass, g_QMSItemPropertiesInitMid, args);
}

/*
 * Account
 */

NGS_JNI_FUNC(jstring, accountGetFirstName)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsAccountGetFirstName());
}

NGS_JNI_FUNC(jstring, accountGetLastName)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsAccountGetLastName());
}

NGS_JNI_FUNC(jstring, accountGetEmail)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsAccountGetEmail());
}

NGS_JNI_FUNC(jstring, accountBitmapPath)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    return env->NewStringUTF(ngsAccountBitmapPath());
}

NGS_JNI_FUNC(jboolean, accountIsAuthorized)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    return ngsAccountIsAuthorized() ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(void, accountExit)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    return ngsAccountExit();
}

NGS_JNI_FUNC(jboolean, accountIsFuncAvailable)(JNIEnv *env, jobject thisObj, jstring application,
        jstring function)
{
    ngsUnused(thisObj);
    return ngsAccountIsFuncAvailable(jniString(env, application).c_str(),
            jniString(env, function).c_str()) ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, accountSupported)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    return ngsAccountSupported() ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, accountUpdateUserInfo)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    return ngsAccountUpdateUserInfo() ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, accountUpdateSupportInfo)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    return ngsAccountUpdateSupportInfo() ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jlong, storeGetTracksTable)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    return reinterpret_cast<jlong>(ngsStoreGetTracksTable(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(jlong, trackGetPointsTable)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    return reinterpret_cast<jlong>(ngsTrackGetPointsTable(reinterpret_cast<CatalogObjectH>(object)));
}

NGS_JNI_FUNC(jboolean, trackIsRegistered)(JNIEnv *env, jobject thisObj)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    return ngsTrackIsRegistered() == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(void, trackSync)(JNIEnv *env, jobject thisObj, jlong object, jint maxPointCount)
{
    ngsUnused(thisObj);
    ngsUnused(env);
    ngsTrackSync(reinterpret_cast<CatalogObjectH>(object), maxPointCount);
}

NGS_JNI_FUNC(jobjectArray, trackGetList)(JNIEnv *env, jobject thisObj, jlong object)
{
    ngsUnused(thisObj);
    int counter = 0;
    ngsTrackInfo *list = ngsTrackGetList(reinterpret_cast<CatalogObjectH>(object));
    std::vector<jobject> obArray;
    if(list) {
        while (list[counter].startTimeStamp != -1) {
            jvalue args[4];
            args[0].l = env->NewStringUTF(list[counter].name);
            args[1].j = list[counter].startTimeStamp;
            args[2].j = list[counter].stopTimeStamp;
            args[3].j = list[counter].count;
            obArray.push_back(env->NewObjectA(g_TrackInfoClass, g_TrackInfoInitMid, args));
            counter++;
        }
        ngsFree(list);
    }

    jobjectArray array = env->NewObjectArray(static_cast<jsize>(obArray.size()), g_TrackInfoClass, nullptr);
    for(int i = 0; i < obArray.size(); ++i) {
        env->SetObjectArrayElement(array, i, obArray[i]);
    }
    return array;
}

NGS_JNI_FUNC(jboolean, trackAddPoint)(JNIEnv *env, jobject thisObj, jlong object, jstring name, jdouble x, jdouble y,
        jdouble z, jfloat acc, jfloat speed, jfloat course, jlong timeStamp, jint satCount, jboolean newTrack,
        jboolean newSegment)
{
    ngsUnused(thisObj);
    return ngsTrackAddPoint(reinterpret_cast<CatalogObjectH>(object), jniString(env, name).c_str(), x, y, z, acc,
            speed, course, timeStamp, satCount, newTrack, newSegment) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}

NGS_JNI_FUNC(jboolean, trackDeletePoints)(JNIEnv *env, jobject thisObj, jlong object, jlong start, jlong stop)
{
    ngsUnused(env);
    ngsUnused(thisObj);
    return ngsTrackDeletePoints(reinterpret_cast<CatalogObjectH>(object), start, stop) == 1 ? NGS_JNI_TRUE : NGS_JNI_FALSE;
}