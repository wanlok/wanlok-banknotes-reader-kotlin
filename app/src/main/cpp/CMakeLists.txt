# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project(BanknotesReader)

# Cross-platform Vuforia core, shared with the iOS app, sits alongside app/ at the repo root
set(CROSS_PLATFORM_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../CrossPlatform)

# Vuforia SDK headers/lib are copied into the repo (not referenced from an external SDK download)
set(VUFORIA_SDK_DIR ${CMAKE_CURRENT_LIST_DIR}/vuforia-sdk)

find_library(ANDROID_LIBRARY android)
find_library(GLES3_LIBRARY GLESv3)
find_library(LOG_LIBRARY log)

# Locate the Vuforia Engine shared library, packaged into the APK from app/src/main/jniLibs
add_library(VuforiaEngine SHARED IMPORTED)
set_property(TARGET VuforiaEngine PROPERTY IMPORTED_LOCATION
             ${CMAKE_CURRENT_LIST_DIR}/../jniLibs/${ANDROID_ABI}/libVuforiaEngine.so)

add_library(banknotesreader SHARED
            # Cross-platform source
            ${CROSS_PLATFORM_DIR}/AppController.cpp

            # Android native sources
            GLESRenderer.cpp
            GLESUtils.cpp
            VuforiaWrapper.cpp
)

target_include_directories(banknotesreader PUBLIC
                           ${CROSS_PLATFORM_DIR}
                           ${VUFORIA_SDK_DIR}/include
)

target_compile_features(banknotesreader PUBLIC cxx_std_17)

target_link_libraries(banknotesreader
                      ${ANDROID_LIBRARY}
                      ${LOG_LIBRARY}
                      ${GLES3_LIBRARY}
                      VuforiaEngine
)
