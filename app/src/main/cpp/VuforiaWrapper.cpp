/*===============================================================================
Copyright (c) 2024 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#include <jni.h>

#include "GLESRenderer.h"
#include <AppController.h>
#include <Log.h>

#include <VuforiaEngine/VuforiaEngine.h>

#include <GLES3/gl31.h>

#include <string>
#include <vector>

// Cross-platform AppController providing high level Vuforia Engine operations
AppController controller;

/// JVM pointer captured below and consumed in the cross-platform code
void* javaVM;

// Struct to hold data that we need to store between calls
struct
{
    JavaVM* vm = nullptr;
    /// Global ref to the VuforiaWorker instance that initAR was called on; callbacks are invoked on it
    jobject callbackObject = nullptr;
    /// Global ref to the Activity, required by Vuforia's Android platform configuration
    jobject activity = nullptr;
    jmethodID presentErrorMethodID = nullptr;
    jmethodID initDoneMethodID = nullptr;
    jmethodID detectionMethodID = nullptr;

    GLESRenderer renderer;
} gWrapperData;


// JNI Implementation
#ifdef __cplusplus
extern "C"
{
#endif

/// Present an error string to the Kotlin side by calling VuforiaWorker.presentError
void callPresentError(const char* errorString);


/// Called by JNI binding when the client code loads the library
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* /* reserved */)
{
    LOG("JNI_OnLoad");

    javaVM = vm;
    gWrapperData.vm = vm;

    return JNI_VERSION_1_6;
}


JNIEXPORT void JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_initAR(JNIEnv* env, jobject thiz, jobject activity, jstring fileName,
                                                      jobjectArray targetNames)
{
    gWrapperData.callbackObject = env->NewGlobalRef(thiz);
    gWrapperData.activity = env->NewGlobalRef(activity);

    jclass clazz = env->GetObjectClass(thiz);
    gWrapperData.presentErrorMethodID = env->GetMethodID(clazz, "presentError", "(Ljava/lang/String;)V");
    gWrapperData.initDoneMethodID = env->GetMethodID(clazz, "initDone", "()V");
    gWrapperData.detectionMethodID = env->GetMethodID(clazz, "onDetection", "(Ljava/lang/String;)V");
    env->DeleteLocalRef(clazz);

    AppController::InitConfig initConfig;
    initConfig.vbRenderBackend = VuRenderVBBackendType::VU_RENDER_VB_BACKEND_GLES3;
    initConfig.appData = gWrapperData.activity;

    initConfig.errorMessageCallback = [](const char* errorString) {
        LOG("Error message callback invoked. Message: %s", errorString);
        callPresentError(errorString);
    };
    initConfig.vuforiaEngineErrorCallback = [](VuErrorCode errorCode) {
        LOG("Vuforia engine error callback invoked. Error code: 0x%02x", errorCode);

        switch (errorCode)
        {
            case VU_ENGINE_ERROR_INVALID_LICENSE:
                callPresentError("License key validation has failed, Engine has been stopped.");
                break;
            case VU_ENGINE_ERROR_CAMERA_DEVICE_LOST:
                callPresentError("Camera device lost (the device has been disconnected or has become unavailable for another reason)");
                break;
            default:
                LOG("Got an unexpected Engine error code 0x%02x", errorCode);
                break;
        }
    };
    initConfig.initDoneCallback = []() {
        JNIEnv* env = nullptr;
        if (gWrapperData.vm->GetEnv((void**)&env, JNI_VERSION_1_6) == 0)
        {
            env->CallVoidMethod(gWrapperData.callbackObject, gWrapperData.initDoneMethodID);
        }
    };
    initConfig.detectionCallback = [](const char* targetName) {
        JNIEnv* env = nullptr;
        if (gWrapperData.vm->GetEnv((void**)&env, JNI_VERSION_1_6) == 0)
        {
            jstring name = targetName != nullptr ? env->NewStringUTF(targetName) : nullptr;
            env->CallVoidMethod(gWrapperData.callbackObject, gWrapperData.detectionMethodID, name);
            if (name != nullptr)
            {
                env->DeleteLocalRef(name);
            }
        }
    };

    const char* fileNameChars = env->GetStringUTFChars(fileName, nullptr);
    std::string fileNameStr(fileNameChars);
    env->ReleaseStringUTFChars(fileName, fileNameChars);

    // Marshal the Java String[] into a char** for AppController::createObservers, keeping the
    // backing std::strings alive for the duration of this call.
    jsize targetCount = env->GetArrayLength(targetNames);
    std::vector<std::string> targetNameStorage;
    std::vector<char*> targetNamePointers;
    targetNameStorage.reserve(targetCount);
    targetNamePointers.reserve(targetCount);
    for (jsize i = 0; i < targetCount; ++i)
    {
        auto jTargetName = static_cast<jstring>(env->GetObjectArrayElement(targetNames, i));
        const char* targetNameChars = env->GetStringUTFChars(jTargetName, nullptr);
        targetNameStorage.emplace_back(targetNameChars);
        env->ReleaseStringUTFChars(jTargetName, targetNameChars);
        env->DeleteLocalRef(jTargetName);
    }
    for (auto& name : targetNameStorage)
    {
        targetNamePointers.push_back(const_cast<char*>(name.c_str()));
    }

    controller.initAR(initConfig, AppController::IMAGE_TARGET_ID, const_cast<char*>(fileNameStr.c_str()), targetNamePointers.data(),
                      static_cast<int>(targetCount));
}


JNIEXPORT jboolean JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_startAR(JNIEnv* /* env */, jobject /* thiz */)
{
    return controller.startAR() ? JNI_TRUE : JNI_FALSE;
}


JNIEXPORT void JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_stopAR(JNIEnv* /* env */, jobject /* thiz */)
{
    controller.stopAR();
}


JNIEXPORT void JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_deinitAR(JNIEnv* env, jobject /* thiz */)
{
    controller.deinitAR();

    if (gWrapperData.activity != nullptr)
    {
        env->DeleteGlobalRef(gWrapperData.activity);
        gWrapperData.activity = nullptr;
    }
    if (gWrapperData.callbackObject != nullptr)
    {
        env->DeleteGlobalRef(gWrapperData.callbackObject);
        gWrapperData.callbackObject = nullptr;
    }
}


JNIEXPORT jboolean JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_isARStarted(JNIEnv* /* env */, jobject /* thiz */)
{
    return controller.isARStarted() ? JNI_TRUE : JNI_FALSE;
}


JNIEXPORT void JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_cameraPerformAutoFocus(JNIEnv* /* env */, jobject /* thiz */)
{
    controller.cameraPerformAutoFocus();
}


JNIEXPORT void JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_cameraRestoreAutoFocus(JNIEnv* /* env */, jobject /* thiz */)
{
    controller.cameraRestoreAutoFocus();
}


JNIEXPORT void JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_initRendering(JNIEnv* /* env */, jobject /* thiz */)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    if (!gWrapperData.renderer.init())
    {
        LOG("Error initialising rendering");
    }
}


JNIEXPORT jboolean JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_configureRendering(JNIEnv* /* env */, jobject /* thiz */, jint width, jint height,
                                                                  jint orientation, jint rotation)
{
    std::vector<int> androidOrientation{ orientation, rotation };
    return controller.configureRendering(width, height, androidOrientation.data()) ? JNI_TRUE : JNI_FALSE;
}


JNIEXPORT jboolean JNICALL
Java_com_wanlok_banknotesreader_VuforiaWorker_renderFrame(JNIEnv* /* env */, jobject /* thiz */)
{
    if (!controller.isARStarted())
    {
        return JNI_FALSE;
    }

    // Clear colour and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int vbTextureUnit = 0;
    VuRenderVideoBackgroundData renderVideoBackgroundData;
    renderVideoBackgroundData.renderData = nullptr;
    renderVideoBackgroundData.textureData = nullptr;
    renderVideoBackgroundData.textureUnitData = &vbTextureUnit;
    double viewport[6];
    if (controller.prepareToRender(viewport, &renderVideoBackgroundData))
    {
        // Set viewport for current view
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

        auto renderState = controller.getRenderState();
        gWrapperData.renderer.renderVideoBackground(renderState.vbProjectionMatrix, renderState.vbMesh->pos, renderState.vbMesh->tex,
                                                    renderState.vbMesh->numFaces, renderState.vbMesh->faceIndices, vbTextureUnit);

        VuMatrix44F trackableProjection;
        VuMatrix44F trackableModelView;
        VuMatrix44F trackableScaledModelView;
        if (controller.getImageTargetResult(trackableProjection, trackableModelView, trackableScaledModelView))
        {
            gWrapperData.renderer.renderImageTarget(trackableProjection, trackableScaledModelView);
        }
    }

    controller.finishRender();

    return JNI_TRUE;
}


void
callPresentError(const char* errorString)
{
    JNIEnv* env = nullptr;
    if (gWrapperData.vm->GetEnv((void**)&env, JNI_VERSION_1_6) == 0)
    {
        jstring error = env->NewStringUTF(errorString);
        env->CallVoidMethod(gWrapperData.callbackObject, gWrapperData.presentErrorMethodID, error);
        env->DeleteLocalRef(error);
    }
}

#ifdef __cplusplus
}
#endif
