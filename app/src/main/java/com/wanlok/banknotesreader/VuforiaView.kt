package com.wanlok.banknotesreader

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.Surface
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/// GLSurfaceView-based camera view; mirrors iOS's VuforiaView.swift (CADisplayLink + CAMetalLayer),
/// using GLSurfaceView.Renderer's onSurfaceCreated/onSurfaceChanged/onDrawFrame instead.
/// Unlike VuforiaWorker.swift/VuforiaView.swift, no "started" flag is mirrored across the two
/// classes here: worker.isARStarted() is a cheap native query, so onDrawFrame asks it directly
/// each frame instead of relying on a Kotlin-side flag kept in sync by a callback.
class VuforiaView(context: Context, private val worker: VuforiaWorker) : GLSurfaceView(context), GLSurfaceView.Renderer {

    private var width = 0
    private var height = 0
    private var configureNeeded = false
    private var lastRotation = Surface.ROTATION_0

    init {
        setEGLContextClientVersion(3)
        setRenderer(this)
    }

    override fun onSurfaceCreated(unused: GL10, config: EGLConfig) {
        worker.initRendering()
    }

    override fun onSurfaceChanged(unused: GL10, width: Int, height: Int) {
        this.width = width
        this.height = height
        configureNeeded = true
    }

    override fun onDrawFrame(unused: GL10) {
        if (!worker.isARStarted()) {
            return
        }

        val rotation = display?.rotation ?: Surface.ROTATION_0
        if (configureNeeded || rotation != lastRotation) {
            configureNeeded = false
            lastRotation = rotation
            worker.configureRendering(width, height, resources.configuration.orientation, rotation)
        }

        worker.renderFrame()
    }
}