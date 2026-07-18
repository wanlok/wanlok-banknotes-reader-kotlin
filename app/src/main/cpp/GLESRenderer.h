/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#ifndef _VUFORIA_GLESRENDERER_H_
#define _VUFORIA_GLESRENDERER_H_

// clang-format off
#include <GLES3/gl31.h>
#include <GLES2/gl2ext.h>
// clang-format on

#include <VuforiaEngine/VuforiaEngine.h>

/// Class to encapsulate OpenGLES rendering for the app.
/// Only Image Targets are used by this app, so this renderer only draws the
/// camera video background and a bounding box on the detected target.
class GLESRenderer
{
public:
    /// Initialize the renderer ready for use
    bool init();

    /// Render the video background
    void renderVideoBackground(const VuMatrix44F& projectionMatrix, const float* vertices, const float* textureCoordinates,
                               const int numTriangles, const unsigned int* indices, int textureUnit);

    /// Render a bounding box augmentation on an Image Target
    void renderImageTarget(VuMatrix44F& projectionMatrix, VuMatrix44F& scaledModelViewMatrix);

private: // data members
    // For video background rendering
    GLuint mVbShaderProgramID = 0;
    GLint mVbVertexPositionHandle = 0;
    GLint mVbTextureCoordHandle = 0;
    GLint mVbMvpMatrixHandle = 0;
    GLint mVbTexSampler2DHandle = 0;

    // For image target bounding box rendering
    GLuint mUniformColorShaderProgramID = 0;
    GLint mUniformColorVertexPositionHandle = 0;
    GLint mUniformColorMvpMatrixHandle = 0;
    GLint mUniformColorColorHandle = 0;
};

#endif //_VUFORIA_GLESRENDERER_H_
