/*===============================================================================
Copyright (c) 2022 PTC. All rights reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#include "AppController.h"

#include "Log.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <string>


#ifdef VU_PLATFORM_ANDROID
/// JVM pointer captured in the VuforiaWrapper for Android
extern void* javaVM;
#endif // VU_PLATFORM_ANDROID


namespace
{
// clang-format off
constexpr char licenseKey[] = "Ac92ycn/////AAABmSp7ivF0HkrBjEltn06MQ/BmTa4g4FYs8CTHhS1Q84v8GvTzo8aYUVtvDbuVNs8BofmUOYfiyqgtGu8B1BOhr+UWAKtshBRNVnP0txBWZbrc3FVo/zZazcWv+bVZstvIeiXBxpP4tYGs3OI61z4cK6BiY7tPU70LRclSX/3yL92OQcQlwCVNShm0pWU6SdV7W+c+JiuyJ4gk/eVau1t7oIvyuHuQHv5xsgQub51WZkFMba19PNZHp4Ko/ACwAiAeDTHa2TuwwkjJ9u6srjuw6VoXz17fQq7nKKqAxCC9qFo6lOr5wvljELFjt5W+JW05KYUahd+eJBB5GGoiR5h27AIEfV2ptd0k6KJ2G13BhAqG";
// clang-format on

constexpr float NEAR_PLANE = 0.01f;
constexpr float FAR_PLANE = 5.f;
}


/// Helper macro to check results of Vuforia Engine calls that are expected to succeed
#define REQUIRE_SUCCESS(command)                     \
    {                                                \
        auto vu_result_appsupport_ = command;        \
        (void)vu_result_appsupport_;                 \
        assert(vu_result_appsupport_ == VU_SUCCESS); \
    }


/*===============================================================================
 AppController public methods
 ===============================================================================*/

void
AppController::initAR(const InitConfig& initConfig, int target, char* fileName, char** targetNames, int targetCount)
{
    mVbRenderBackend = initConfig.vbRenderBackend;
    mErrorMessageCallback = initConfig.errorMessageCallback;
    mVuforeEngineErrorCallback = initConfig.vuforiaEngineErrorCallback;
    mInitDoneCallback = initConfig.initDoneCallback;
    mDetectionCallback = initConfig.detectionCallback;
    mTarget = target;

    mGuideViewModelTarget = nullptr;
    
    if (!initVuforiaInternal(initConfig.appData))
    {
        return;
    }

    if (!createObservers(fileName, targetNames, targetCount))
    {
        return;
    }

    mInitDoneCallback();
}


bool
AppController::startAR()
{
    LOG("AppController::startAR");

    // Bail out early if engine instance has not been created yet
    if (mEngine == nullptr)
    {
        LOG("Failed to start Vuforia as no valid engine instance is available");
        return false;
    }

    // Bail out early if engine has already been started
    if (vuEngineIsRunning(mEngine))
    {
        LOG("Failed to start Vuforia as it is already running");
        return false;
    }

    // Get the camera controller to access camera settings
    VuController* cameraController = nullptr;
    REQUIRE_SUCCESS(vuEngineGetCameraController(mEngine, &cameraController));

    // Select the camera mode to the preferred value before starting engine
    if (vuCameraControllerSetActiveVideoMode(cameraController, mCameraVideoMode) != VU_SUCCESS)
    {
        LOG("Failed to set active video mode %d for camera device", static_cast<int>(mCameraVideoMode));
    }

    // Start engine
    if (vuEngineStart(mEngine) != VU_SUCCESS)
    {
        LOG("Failed to start Vuforia");
        return false;
    }

    mARStarted = true;

    // Select the camera focus mode to continuous autofocus
    if (vuCameraControllerSetFocusMode(cameraController, VU_CAMERA_FOCUS_MODE_CONTINUOUSAUTO) != VU_SUCCESS)
    {
        LOG("Failed to select focus mode %d for camera device", static_cast<int>(VU_CAMERA_FOCUS_MODE_CONTINUOUSAUTO));
    }

    LOG("Successfully started Vuforia");
    return true;
}


bool
AppController::stopAR()
{
    LOG("AppController::stopAR");

    // Bail out early if engine instance has not been created yet
    if (mEngine == nullptr)
    {
        LOG("Failed to stop Vuforia as no valid engine instance is available");
        return false;
    }

    // Bail out early if engine has not been started yet
    if (!vuEngineIsRunning(mEngine))
    {
        LOG("Failed to stop Vuforia as it is currently not running");
        return false;
    }

    mARStarted = false;

    // Stop engine
    if (vuEngineStop(mEngine) != VU_SUCCESS)
    {
        LOG("Failed to stop Vuforia");
        return false;
    }

    LOG("Successfully stopped Vuforia");
    return true;
}


void
AppController::deinitAR()
{
    // Bail out early if engine instance has not been created yet
    if (mEngine == nullptr)
    {
        LOG("Failed to deinitialize Vuforia as no engine instance is available");
        return;
    }

    stopAR();

    destroyObservers();

    // Destroy engine instance
    if (vuEngineDestroy(mEngine) != VU_SUCCESS)
    {
        LOG("Failed to destroy engine instance");
        return;
    }

    // Invalidate engine instance
    mEngine = nullptr;

    // Invalidate render and platform controllers
    mRenderController = nullptr;
    mPlatformController = nullptr;
}


void
AppController::cameraPerformAutoFocus()
{
    if (!mARStarted)
    {
        return;
    }

    VuController* cameraController = nullptr;
    if (vuEngineGetCameraController(mEngine, &cameraController) != VU_SUCCESS)
    {
        LOG("Error attempting to perform autofocus, failed to get camera controller");
        return;
    }

    if (vuCameraControllerSetFocusMode(cameraController, VU_CAMERA_FOCUS_MODE_TRIGGERAUTO) != VU_SUCCESS)
    {
        LOG("Error attempting to perform autofocus, failed to set focus mode");
    }
}


void
AppController::cameraRestoreAutoFocus()
{
    if (!mARStarted)
    {
        return;
    }

    VuController* cameraController = nullptr;
    if (vuEngineGetCameraController(mEngine, &cameraController) != VU_SUCCESS)
    {
        LOG("Error attempting to perform autofocus, failed to get camera controller");
        return;
    }

    if (vuCameraControllerSetFocusMode(cameraController, VU_CAMERA_FOCUS_MODE_CONTINUOUSAUTO) != VU_SUCCESS)
    {
        LOG("Error attempting to perform autofocus, failed to set focus mode");
    }
}


bool
AppController::configureRendering(int width, int height, void* orientation)
{
    if (!mARStarted)
    {
        return false;
    }

    if (width <= 0 || height <= 0)
    {
        LOG("Invalid screen dimensions");
        return false;
    }

    VuViewOrientation vuOrientation;
    if (vuPlatformControllerConvertPlatformViewOrientation(mPlatformController, orientation, &vuOrientation) != VU_SUCCESS)
    {
        LOG("Failed to convert the platform-specific orientation descriptor to Vuforia view orientation");
        return false;
    }

    if (vuPlatformControllerSetViewOrientation(mPlatformController, vuOrientation) != VU_SUCCESS)
    {
        LOG("Failed to set orientation");
        return false;
    }

    mDisplayAspectRatio = (float)width / height;

    // Set the latest render view configuration in Vuforia
    VuRenderViewConfig rvConfig;
    rvConfig.resolution.data[0] = width;
    rvConfig.resolution.data[1] = height;

    if (vuRenderControllerSetRenderViewConfig(mRenderController, &rvConfig) != VU_SUCCESS)
    {
        LOG("Failed to set render view configuration");
    }

    return true;
}


bool
AppController::getVideoBackgroundTextureSize(VuVector2I& textureSize)
{
    VuVideoBackgroundViewInfo vbViewInfo;
    if (vuRenderControllerGetVideoBackgroundViewInfo(mRenderController, &vbViewInfo) != VU_SUCCESS)
    {
        LOG("Error getting video background view info");
        return false;
    }

    textureSize = vbViewInfo.vBTextureSize;
    return true;
}


bool
AppController::prepareToRender(double* viewport, VuRenderVideoBackgroundData* renderData)
{
    if (vuEngineAcquireLatestState(mEngine, &mVuforiaState) != VU_SUCCESS)
    {
        LOG("Error getting state");
        return false;
    }

    if (vuStateHasCameraFrame(mVuforiaState) != VU_TRUE)
    {
        return false;
    }

    if (vuStateGetRenderState(mVuforiaState, &mCurrentRenderState) != VU_SUCCESS)
    {
        LOG("Error getting render state");
        return false;
    }

    if (!mCurrentRenderState.vbMesh)
    {
        return false;
    }

    viewport[0] = mCurrentRenderState.viewport.data[0];
    viewport[1] = mCurrentRenderState.viewport.data[1];
    viewport[2] = mCurrentRenderState.viewport.data[2];
    viewport[3] = mCurrentRenderState.viewport.data[3];
    viewport[4] = 0.0f;
    viewport[5] = 1.0f;

    if (vuRenderControllerUpdateVideoBackgroundTexture(mRenderController, mVuforiaState, renderData) != VU_SUCCESS)
    {
        LOG("Error updating video background texture");
        return false;
    }

    updateDevicePose();

    return true;
}


void
AppController::finishRender()
{
    // Check for device tracker relocalizing for too long and reset if needed
    if (mLatestDevicePoseData.poseStatus == VU_OBSERVATION_POSE_STATUS_LIMITED &&
        mLatestDevicePoseData.poseStatusInfo == VU_DEVICE_POSE_OBSERVATION_STATUS_INFO_RELOCALIZING)
    {
        using namespace std::chrono;

        // Start timing if we have just entered relocalizing state
        if (!mTimingRelocalizingState)
        {
            mEnteredRelocalizingState = steady_clock::now();
        }
        mTimingRelocalizingState = true;

        // Check whether we have been relocalizing for longer than the threshold
        auto relocalizingFor = duration_cast<seconds>(steady_clock::now() - mEnteredRelocalizingState);
        if (relocalizingFor.count() > MAX_RELOCALIZING_SECONDS)
        {
            mTimingRelocalizingState = false;
            VuResult resetResult = vuEngineResetWorldTracking(mEngine);
            LOG("%s reset world tracking", resetResult == VU_SUCCESS ? "Successfully" : "Failed to");
        }
    }
    else
    {
        mTimingRelocalizingState = false;
    }

    // Clean up and release the Vuforia state
    if (mVuforiaState != nullptr && vuStateRelease(mVuforiaState) != VU_SUCCESS)
    {
        LOG("Error releasing the Vuforia state");
    }
    mVuforiaState = nullptr;
}


bool
AppController::getOrigin(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix)
{
    if (mLatestDevicePoseData.poseStatus != VU_OBSERVATION_POSE_STATUS_NO_POSE)
    {
        projectionMatrix = mCurrentRenderState.projectionMatrix;
        modelViewMatrix = mCurrentRenderState.viewMatrix;
        return true;
    }

    return false;
}

bool isTargetInScreen(const VuMatrix44F& projectionMatrix, const VuMatrix44F& modelViewMatrix)
{
    VuVector4F ndc;
    VuVector3F targetCenter = {0, 0, 0};
    
    VuMatrix44F mvp = vuMatrix44FMultiplyMatrix(projectionMatrix, modelViewMatrix);

    ndc.data[0] = mvp.data[0]*targetCenter.data[0] + mvp.data[4]*targetCenter.data[1] +
                  mvp.data[8]*targetCenter.data[2] + mvp.data[12];
    ndc.data[1] = mvp.data[1]*targetCenter.data[0] + mvp.data[5]*targetCenter.data[1] +
                  mvp.data[9]*targetCenter.data[2] + mvp.data[13];
    ndc.data[2] = mvp.data[2]*targetCenter.data[0] + mvp.data[6]*targetCenter.data[1] +
                  mvp.data[10]*targetCenter.data[2] + mvp.data[14];
    ndc.data[3] = mvp.data[3]*targetCenter.data[0] + mvp.data[7]*targetCenter.data[1] +
                  mvp.data[11]*targetCenter.data[2] + mvp.data[15];

    ndc.data[0] /= ndc.data[3];
    ndc.data[1] /= ndc.data[3];
    ndc.data[2] /= ndc.data[3];

    return (ndc.data[0] >= -1.0f && ndc.data[0] <= 1.0f &&
            ndc.data[1] >= -1.0f && ndc.data[1] <= 1.0f &&
            ndc.data[2] >= 0.0f  && ndc.data[2] <= 1.0f);
}

bool
AppController::getImageTargetResult(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, VuMatrix44F& scaledModelViewMatrix)
{
    bool result = false;
    
    const char* targetName = nullptr;

    if (mTarget != IMAGE_TARGET_ID)
    {
        return false;
    }

    VuObservationList* observationList = nullptr;
    REQUIRE_SUCCESS(vuObservationListCreate(&observationList));

    if (vuStateGetImageTargetObservations(mVuforiaState, observationList) != VU_SUCCESS)
    {
        LOG("Error getting image target observations");
        REQUIRE_SUCCESS(vuObservationListDestroy(observationList));
        return false;
    }

    int numObservations = 0;
    REQUIRE_SUCCESS(vuObservationListGetSize(observationList, &numObservations));

    if (numObservations > 0)
    {
        VuObservation* observation = nullptr;
        if (vuObservationListGetElement(observationList, 0, &observation) == VU_SUCCESS)
        {
            assert(observation);
            assert(vuObservationIsType(observation, VU_OBSERVATION_IMAGE_TARGET_TYPE) == VU_TRUE);
            assert(vuObservationHasPoseInfo(observation) == VU_TRUE);

            VuPoseInfo poseInfo;
            REQUIRE_SUCCESS(vuObservationGetPoseInfo(observation, &poseInfo));

            VuImageTargetObservationTargetInfo imageTargetInfo;
            REQUIRE_SUCCESS(vuImageTargetObservationGetTargetInfo(observation, &imageTargetInfo));

            if (poseInfo.poseStatus != VU_OBSERVATION_POSE_STATUS_NO_POSE)
            {
                projectionMatrix = mCurrentRenderState.projectionMatrix;

                // Compute model-view matrix
                auto modelMatrix = poseInfo.pose;
                modelViewMatrix = vuMatrix44FMultiplyMatrix(mCurrentRenderState.viewMatrix, modelMatrix);

                // Calculate a scaled modelViewMatrix for rendering a unit bounding box
                // z-dimension will be zero for planar target
                // set it here to the larger dimension so that
                // a 3D augmentation can be shown
                VuVector3F scale;
                scale.data[0] = imageTargetInfo.size.data[0];
                scale.data[1] = imageTargetInfo.size.data[1];
                scale.data[2] = std::max(scale.data[0], scale.data[1]);
                scaledModelViewMatrix = vuMatrix44FScale(scale, modelViewMatrix);

                if (isTargetInScreen(projectionMatrix, modelViewMatrix))
                {
                    targetName = imageTargetInfo.name;
                }
                result = true;
            }
            
        }
    }
    

    REQUIRE_SUCCESS(vuObservationListDestroy(observationList));
    
    mDetectionCallback(targetName);
    
    return result;
}


bool
AppController::getModelTargetResult(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, VuMatrix44F& scaledModelViewMatrix)
{
    // Model Targets are not used by this app; only Image Targets are tracked.
    return false;
}


bool
AppController::getModelTargetGuideView(VuMatrix44F& projectionMatrix, VuMatrix44F& modelViewMatrix, VuImageInfo& guideViewImageInfo,
                                       VuBool& guideViewImageHasChanged)
{
    if (mGuideViewModelTarget == nullptr)
    {
        return false;
    }

    VuCameraIntrinsics cameraIntrinsics;
    if (vuStateGetCameraIntrinsics(mVuforiaState, &cameraIntrinsics) != VU_SUCCESS)
    {
        return false;
    }
    auto fov = vuCameraIntrinsicsGetFov(&cameraIntrinsics);

    if (vuGuideViewIsImageOutdated(mGuideViewModelTarget, &guideViewImageHasChanged) != VU_SUCCESS)
    {
        return false;
    }

    VuImage* guideViewImage = nullptr;
    if (vuGuideViewGetImage(mGuideViewModelTarget, &guideViewImage) != VU_SUCCESS)
    {
        return false;
    }

    if (vuImageGetImageInfo(guideViewImage, &guideViewImageInfo) != VU_SUCCESS)
    {
        LOG("Error getting image info for guide view");
        return false;
    }

    float guideViewAspectRatio = (float)guideViewImageInfo.width / guideViewImageInfo.height;

    float planeDistance = 0.01f;
    float fieldOfView = fov.data[1];
    float nearPlaneHeight = 1.0f * planeDistance * std::tanf(fieldOfView * 0.5f);
    float nearPlaneWidth = nearPlaneHeight * mDisplayAspectRatio;

    float planeWidth;
    float planeHeight;
    if (guideViewAspectRatio >= 1.0f && mDisplayAspectRatio >= 1.0f) // guideview landscape, camera landscape
    {
        // scale so that the long side of the camera (width)
        // is the same length as guideview width
        planeWidth = nearPlaneWidth;
        planeHeight = planeWidth / guideViewAspectRatio;
    }

    else if (guideViewAspectRatio < 1.0f && mDisplayAspectRatio < 1.0f) // guideview portrait, camera portrait
    {
        // scale so that the long side of the camera (height)
        // is the same length as guideview height
        planeHeight = nearPlaneHeight;
        planeWidth = planeHeight * guideViewAspectRatio;
    }
    else if (mDisplayAspectRatio < 1.0f) // guideview landscape, camera portrait
    {
        // scale so that the long side of the camera (height)
        // is the same length as guideview width
        planeWidth = nearPlaneHeight;
        planeHeight = planeWidth / guideViewAspectRatio;
    }
    else // guideview portrait, camera landscape
    {
        // scale so that the long side of the camera (width)
        // is the same length as guideview height
        planeHeight = nearPlaneWidth;
        planeWidth = planeHeight * guideViewAspectRatio;
    }

    // normalize world space plane sizes into view space again
    VuVector2F scale = { 2 * planeWidth / nearPlaneWidth, 2 * planeHeight / nearPlaneHeight };

    projectionMatrix = vuIdentityMatrix44F();
    modelViewMatrix = vuIdentityMatrix44F();

    modelViewMatrix = vuMatrix44FScale(VuVector3F{ scale.data[0], scale.data[1], 1.0f }, modelViewMatrix);

    return true;
}


/*===============================================================================
 AppController private methods
 ===============================================================================*/

bool
AppController::initVuforiaInternal(void* appData)
{
    LOG("AppController::initEngine");

    // Bail out early if an engine instance has already been created (apps must call deinitEngine first before calling reinitialization)
    if (mEngine != nullptr)
    {
        LOG("Failed to initialize Vuforia as a valid engine instance already exists");
        return false;
    }

    // Create engine configuration data structure
    VuEngineConfigSet* configSet = nullptr;
    REQUIRE_SUCCESS(vuEngineConfigSetCreate(&configSet));

    // Add license key to engine configuration
    auto licenseConfig = vuLicenseConfigDefault();
    licenseConfig.key = licenseKey;
    if (vuEngineConfigSetAddLicenseConfig(configSet, &licenseConfig) != VU_SUCCESS)
    {
        // Clean up before exiting
        REQUIRE_SUCCESS(vuEngineConfigSetDestroy(configSet));

        LOG("Failed to init Vuforia, license key could not be added to configuration");
        mErrorMessageCallback("Vuforia failed to initialize because the license key could not be added to the configuration");
        return false;
    }

    // Create default render configuration (may be overwritten by platform-specific settings)
    // The default selects the platform preferred rendering backend
    auto renderConfig = vuRenderConfigDefault();
    renderConfig.vbRenderBackend = mVbRenderBackend;

#if defined(VU_PLATFORM_ANDROID)
    // Add platform-specific engine configuration
    VuResult platformConfigResult = VU_SUCCESS;

    // Set Android Activity owning the Vuforia Engine in platform-specific configuration
    auto vuPlatformConfig_Android = vuPlatformAndroidConfigDefault();
    vuPlatformConfig_Android.activity = appData;
    vuPlatformConfig_Android.javaVM = javaVM;

    // Add platform-specific configuration to engine configuration set
    platformConfigResult = vuEngineConfigSetAddPlatformAndroidConfig(configSet, &vuPlatformConfig_Android);

    // Check platform configuration result
    if (platformConfigResult != VU_SUCCESS)
    {
        // Clean up before exiting
        REQUIRE_SUCCESS(vuEngineConfigSetDestroy(configSet));

        LOG("Failed to init Vuforia, could not apply platform-specific configuration");
        mErrorMessageCallback("Vuforia failed to initialize, could not apply platform-specific configuration");
        return false;
    }
#elif defined(VU_PLATFORM_IOS)
    // Add platform-specific engine configuration
    VuResult platformConfigResult = VU_SUCCESS;

    // Set display orientation in platform-specific configuration
    auto vuPlatformConfig_iOS = vuPlatformiOSConfigDefault();
    vuPlatformConfig_iOS.interfaceOrientation = appData;

    // Add platform-specific configuration to engine configuration set
    platformConfigResult = vuEngineConfigSetAddPlatformiOSConfig(configSet, &vuPlatformConfig_iOS);

    // Check platform configuration result
    if (platformConfigResult != VU_SUCCESS)
    {
        // Clean up before exiting
        REQUIRE_SUCCESS(vuEngineConfigSetDestroy(configSet));

        LOG("Failed to init Vuforia, could not apply platform-specific configuration");
        mErrorMessageCallback("Vuforia failed to initialize, could not apply platform-specific configuration");
        return false;
    }
#elif defined(VU_PLATFORM_UWP)
    // Add platform-specific engine configuration
    VuResult platformConfigResult = VU_SUCCESS;

    // Set display orientation in platform-specific configuration
    auto vuPlatformConfig_UWP = vuPlatformUWPConfigDefault();
    vuPlatformConfig_UWP.displayOrientation = appData;

    // Add platform-specific configuration to engine configuration set
    platformConfigResult = vuEngineConfigSetAddPlatformUWPConfig(configSet, &vuPlatformConfig_UWP);

    // Check platform configuration result
    if (platformConfigResult != VU_SUCCESS)
    {
        // Clean up before exiting
        REQUIRE_SUCCESS(vuEngineConfigSetDestroy(configSet));

        LOG("Failed to init Vuforia, could not apply platform-specific configuration");
        mErrorMessageCallback("Vuforia failed to initialize, could not apply platform-specific configuration");
        return false;
    }
#else
    (void)appData;
#endif

    // Add rendering-specific engine configuration
    if (vuEngineConfigSetAddRenderConfig(configSet, &renderConfig) != VU_SUCCESS)
    {
        // Clean up before exiting
        REQUIRE_SUCCESS(vuEngineConfigSetDestroy(configSet));

        LOG("Failed to init Vuforia, could not configure rendering");
        mErrorMessageCallback("Vuforia failed to initialize, could not configure rendering");
        return false;
    }

    // Add asynchronous engine error handler
    VuErrorHandlerConfig errorHandlerConfig = vuErrorHandlerConfigDefault();
    errorHandlerConfig.errorHandler = &onEngineError;
    errorHandlerConfig.clientData = this;
    if (vuEngineConfigSetAddErrorHandlerConfig(configSet, &errorHandlerConfig) != VU_SUCCESS)
    {
        // Clean up before exiting
        [[maybe_unused]] auto destroyConfigResult = vuEngineConfigSetDestroy(configSet);
        assert(destroyConfigResult == VU_SUCCESS);

        LOG("Failed to init Vuforia, error handler data could not be added to configuration");
        return false;
    }

    // Create Engine instance
    VuErrorCode errorCode;
    auto engineCreateResult = vuEngineCreate(&mEngine, configSet, &errorCode);

    // Destroy configuration data as we have used it for engine creation
    REQUIRE_SUCCESS(vuEngineConfigSetDestroy(configSet));

    if (engineCreateResult != VU_SUCCESS)
    {
        std::string errorMessage = initErrorToString(errorCode);
        mErrorMessageCallback(errorMessage.c_str());
        return false;
    }

    // Bail out if engine creation has failed
    if (mEngine == nullptr)
    {
        LOG("Failed to init Vuforia, could not create engine instance");
        mErrorMessageCallback("Vuforia initialization failed.");
        return false;
    }

    // Retrieve Vuforia render and platform controllers from engine and cache them (remain valid as long as the engine instance is valid)
    REQUIRE_SUCCESS(vuEngineGetRenderController(mEngine, &mRenderController));
    assert(mRenderController);
    REQUIRE_SUCCESS(vuEngineGetPlatformController(mEngine, &mPlatformController));
    assert(mPlatformController);

    if (vuRenderControllerSetProjectionMatrixNearFar(mRenderController, NEAR_PLANE, FAR_PLANE) != VU_SUCCESS)
    {
        LOG("Error setting clipping planes for projection");
        return false;
    }

    LOG("Successfully initialized Vuforia");
    return true;
}


void
AppController::onEngineError(VuEngineError errorCode, void* clientData)
{
    AppController* appController = static_cast<AppController*>(clientData);
    assert(appController);

    // Delegate the error to the AppController instance for handling
    appController->handleEngineError(errorCode);
}


void
AppController::handleEngineError(VuEngineError errorCode)
{
    switch (errorCode)
    {
        case VU_ENGINE_ERROR_INVALID_LICENSE:
        case VU_ENGINE_ERROR_CAMERA_DEVICE_LOST:
        case VU_ENGINE_ERROR_PLATFORM_FUSION_PROVIDER_INFO_INVALIDATED:
            if (mVuforeEngineErrorCallback)
            {
                mVuforeEngineErrorCallback(errorCode);
            }
            break;
        default:
            LOG("Got an unknown Engine error code %x", errorCode);
            assert(false);
            break;
    }
}


std::string
AppController::initErrorToString(VuErrorCode error)
{
    std::string errorMessage;

    switch (error)
    {
        case VU_ENGINE_CREATION_ERROR_DEVICE_NOT_SUPPORTED:
            errorMessage = "Vuforia failed to initialize because the device is not supported.";
            break;

        case VU_ENGINE_CREATION_ERROR_PERMISSION_ERROR:
            // On most platforms the user must explicitly grant camera access (along with other required permissions).
            // If the access request is denied to any of the required permissions, this code is returned.
            errorMessage = "Vuforia cannot initialize because access to the camera was denied.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_ERROR:
            errorMessage = "Vuforia cannot initialize because a valid license configuration is required.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_CONFIG_MISSING_KEY:
            errorMessage = "Vuforia failed to initialize because the license key is missing.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_CONFIG_INVALID_KEY:
            errorMessage = "Vuforia failed to initialize because the license key is invalid.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_CONFIG_NO_NETWORK_PERMANENT:
            errorMessage = "Vuforia failed to initialize because the license check encountered a permanent network error.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_CONFIG_NO_NETWORK_TRANSIENT:
            errorMessage = "Vuforia failed to initialize because the license check encountered a temporary network error.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_CONFIG_BAD_REQUEST:
            errorMessage = "Vuforia failed to initialize because the request to the license server is malformed, ensure the app has valid "
                           "name and version fields.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_CONFIG_KEY_CANCELED:
            errorMessage = "Vuforia failed to initialize because the license key was canceled.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_CONFIG_PRODUCT_TYPE_MISMATCH:
            errorMessage = "Vuforia failed to initialize because the license key is for the wrong product type.";
            break;

        case VU_ENGINE_CREATION_ERROR_LICENSE_CONFIG_UNKNOWN:
            errorMessage = "Vuforia failed to initialize because the license check encountered an unknown error.";
            break;

        case VU_ENGINE_CREATION_ERROR_RENDER_CONFIG_UNSUPPORTED_BACKEND:
            errorMessage =
                "Vuforia failed to initialize because the requested rendering backend is not supported on this platform or device.";
            break;

        case VU_ENGINE_CREATION_ERROR_RENDER_CONFIG_FAILED_TO_SET_VIDEO_BG_VIEWPORT:
            errorMessage = "Vuforia failed to initialize because the requested videobackground viewport could not be set.";
            break;

        case VU_ENGINE_CREATION_ERROR_INITIALIZATION:
        default:
            errorMessage = "Vuforia initialization failed";
            break;
    }

    return errorMessage;
}


bool
AppController::createObservers(char* fileName, char** targetNames, int targetCount)
{
    auto devicePoseConfig = vuDevicePoseConfigDefault();
    VuDevicePoseCreationError devicePoseCreationError;
    if (vuEngineCreateDevicePoseObserver(mEngine, &mDevicePoseObserver, &devicePoseConfig, &devicePoseCreationError) != VU_SUCCESS)
    {
        LOG("Error creating device pose observer: 0x%02x", devicePoseCreationError);
        return false;
    }

    std::string databasePath = std::string(fileName) + ".xml";
    
    for (int i = 0; i < targetCount; ++i)
    {
        const char* targetName = targetNames[i];
        auto config = vuImageTargetConfigDefault();
        config.databasePath = databasePath.c_str();
        config.targetName = targetName;
        config.activate = VU_TRUE;

        VuObserver* observer = nullptr;
        VuImageTargetCreationError error;
        if (vuEngineCreateImageTargetObserver(mEngine, &observer, &config, &error) == VU_SUCCESS)
        {
            mObjectObservers.push_back(observer);
        }
        else
        {
            LOG("Error creating image target observer for %s (0x%02x)", targetName, error);
        }
    }

    return true;
}


void
AppController::destroyObservers()
{
    for (auto observer : mObjectObservers)
    {
        if (observer != nullptr && vuObserverDestroy(observer) != VU_SUCCESS)
        {
            LOG("Error destroying object observer");
        }
    }
    mObjectObservers.clear();

    if (mDevicePoseObserver != nullptr && vuObserverDestroy(mDevicePoseObserver) != VU_SUCCESS)
    {
        LOG("Error destroying object observer");
    }
    mDevicePoseObserver = nullptr;
}


void
AppController::updateDevicePose()
{
    mLatestDevicePoseData.pose = vuIdentityMatrix44F();
    mLatestDevicePoseData.poseStatus = VU_OBSERVATION_POSE_STATUS_NO_POSE;
    mLatestDevicePoseData.poseStatusInfo = VU_DEVICE_POSE_OBSERVATION_STATUS_INFO_NORMAL;

    VuObservationList* observationList = nullptr;
    REQUIRE_SUCCESS(vuObservationListCreate(&observationList));

    if (vuStateGetDevicePoseObservations(mVuforiaState, observationList) == VU_SUCCESS)
    {
        int numObservations = 0;
        REQUIRE_SUCCESS(vuObservationListGetSize(observationList, &numObservations));

        if (numObservations > 0)
        {
            VuObservation* observation = nullptr;
            if (vuObservationListGetElement(observationList, 0, &observation) == VU_SUCCESS)
            {
                assert(observation);
                assert(vuObservationIsType(observation, VU_OBSERVATION_DEVICE_POSE_TYPE) == VU_TRUE);
                assert(vuObservationHasPoseInfo(observation) == VU_TRUE);

                VuPoseInfo poseInfo;
                REQUIRE_SUCCESS(vuObservationGetPoseInfo(observation, &poseInfo));

                if (poseInfo.poseStatus != VU_OBSERVATION_POSE_STATUS_NO_POSE)
                {
                    // Store latest tracked device pose and pose status
                    mLatestDevicePoseData.pose = poseInfo.pose;
                    mLatestDevicePoseData.poseStatus = poseInfo.poseStatus;

                    // Retrieve device pose-specific status information
                    REQUIRE_SUCCESS(vuDevicePoseObservationGetStatusInfo(observation, &mLatestDevicePoseData.poseStatusInfo));
                }
            }
        }
    }
    else
    {
        LOG("Error getting device pose observations");
    }

    REQUIRE_SUCCESS(vuObservationListDestroy(observationList));
}
