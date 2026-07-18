/*===============================================================================
Copyright (c) 2023 PTC Inc. and/or Its Subsidiary Companies. All Rights Reserved.

Vuforia is a trademark of PTC Inc., registered in the United States and other
countries.
===============================================================================*/

#include "GLESRenderer.h"

#include "GLESUtils.h"
#include "Shaders.h"

#include <Models.h>

bool
GLESRenderer::init()
{
    // Setup for Video Background rendering
    mVbShaderProgramID = GLESUtils::createProgramFromBuffer(textureVertexShaderSrc, textureFragmentShaderSrc);
    mVbVertexPositionHandle = glGetAttribLocation(mVbShaderProgramID, "vertexPosition");
    mVbTextureCoordHandle = glGetAttribLocation(mVbShaderProgramID, "vertexTextureCoord");
    mVbMvpMatrixHandle = glGetUniformLocation(mVbShaderProgramID, "modelViewProjectionMatrix");
    mVbTexSampler2DHandle = glGetUniformLocation(mVbShaderProgramID, "texSampler2D");

    // Setup for Image Target bounding box rendering
    mUniformColorShaderProgramID = GLESUtils::createProgramFromBuffer(uniformColorVertexShaderSrc, uniformColorFragmentShaderSrc);
    mUniformColorVertexPositionHandle = glGetAttribLocation(mUniformColorShaderProgramID, "vertexPosition");
    mUniformColorMvpMatrixHandle = glGetUniformLocation(mUniformColorShaderProgramID, "modelViewProjectionMatrix");
    mUniformColorColorHandle = glGetUniformLocation(mUniformColorShaderProgramID, "uniformColor");

    return true;
}


void
GLESRenderer::renderVideoBackground(const VuMatrix44F& projectionMatrix, const float* vertices, const float* textureCoordinates,
                                    const int numTriangles, const unsigned int* indices, int textureUnit)
{
    GLboolean depthTest = GL_FALSE;
    GLboolean cullTest = GL_FALSE;

    glGetBooleanv(GL_DEPTH_TEST, &depthTest);
    glGetBooleanv(GL_CULL_FACE, &cullTest);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Load the shader and upload the vertex/texcoord/index data
    glUseProgram(mVbShaderProgramID);
    glVertexAttribPointer(static_cast<GLuint>(mVbVertexPositionHandle), 3, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(static_cast<GLuint>(mVbTextureCoordHandle), 2, GL_FLOAT, GL_FALSE, 0, textureCoordinates);

    glUniform1i(mVbTexSampler2DHandle, textureUnit);

    // Render the video background with the custom shader
    // First, we enable the vertex arrays
    glEnableVertexAttribArray(static_cast<GLuint>(mVbVertexPositionHandle));
    glEnableVertexAttribArray(static_cast<GLuint>(mVbTextureCoordHandle));

    // Pass the projection matrix to OpenGL
    glUniformMatrix4fv(mVbMvpMatrixHandle, 1, GL_FALSE, projectionMatrix.data);

    // Then, we issue the render call
    glDrawElements(GL_TRIANGLES, numTriangles * 3, GL_UNSIGNED_INT, indices);

    // Finally, we disable the vertex arrays
    glDisableVertexAttribArray(static_cast<GLuint>(mVbVertexPositionHandle));
    glDisableVertexAttribArray(static_cast<GLuint>(mVbTextureCoordHandle));

    if (depthTest)
        glEnable(GL_DEPTH_TEST);

    if (cullTest)
        glEnable(GL_CULL_FACE);

    GLESUtils::checkGlError("Render video background");
}


void
GLESRenderer::renderImageTarget(VuMatrix44F& projectionMatrix, VuMatrix44F& scaledModelViewMatrix)
{
    VuMatrix44F scaledModelViewProjectionMatrix = vuMatrix44FMultiplyMatrix(projectionMatrix, scaledModelViewMatrix);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float stateLineWidth;
    glGetFloatv(GL_LINE_WIDTH, &stateLineWidth);

    glUseProgram(mUniformColorShaderProgramID);

    glVertexAttribPointer(mUniformColorVertexPositionHandle, 3, GL_FLOAT, GL_TRUE, 0, (const GLvoid*)&squareVertices[0]);

    glEnableVertexAttribArray(mUniformColorVertexPositionHandle);

    glUniformMatrix4fv(mUniformColorMvpMatrixHandle, 1, GL_FALSE, &scaledModelViewProjectionMatrix.data[0]);

    // Draw translucent overlay
    glUniform4f(mUniformColorColorHandle, 0.0, 1.0, 0.0, 0.2);
    glDrawElements(GL_TRIANGLES, NUM_SQUARE_INDEX, GL_UNSIGNED_SHORT, (const GLvoid*)&squareIndices[0]);

    // Draw solid outline
    glUniform4f(mUniformColorColorHandle, 0.0, 1.0, 0.0, 1.0);
    glLineWidth(4.0f);
    glDrawElements(GL_LINES, NUM_SQUARE_WIREFRAME_INDEX, GL_UNSIGNED_SHORT, (const GLvoid*)&squareWireframeIndices[0]);

    glDisableVertexAttribArray(mUniformColorVertexPositionHandle);

    GLESUtils::checkGlError("Render Image Target");

    glLineWidth(stateLineWidth);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}
