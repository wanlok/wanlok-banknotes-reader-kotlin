package com.wanlok.banknotesreader

import android.app.Activity
import android.os.Handler
import android.os.Looper
import android.util.Log

/// Kotlin counterpart to VuforiaWrapper.cpp's JNI bridge; mirrors iOS's VuforiaWorker.swift.
class VuforiaWorker(private val activity: Activity, private val callback: (String?) -> Unit) {

    private val mainHandler = Handler(Looper.getMainLooper())
    private var startedCallback: ((Boolean) -> Unit)? = null

    /// fileName/targetNames are the already-resolved dataset name and ImageTarget names
    /// (dataset download/XML parsing happens upstream of this call).
    /// initAR does blocking Vuforia engine + observer creation, so it runs off the caller's thread.
    fun start(fileName: String, targetNames: Array<String>, startedCallback: (Boolean) -> Unit) {
        this.startedCallback = startedCallback
        Thread { initAR(activity, fileName, targetNames) }.start()
    }

    fun stop() {
        stopAR()
        deinitAR()
    }

    /// Releases the camera without tearing down the engine/observers, so the app doesn't hold
    /// the camera while backgrounded (matches PTC's own sample: VuforiaActivity.kt's onPause
    /// calls stopAR, not the full stop()/deinitAR() teardown this class also exposes).
    fun pause() {
        stopAR()
    }

    /// Reacquires the camera on an already-initialized engine; startAR() is safe to call
    /// again since stopAR() (see [pause]) doesn't destroy mEngine or its observers.
    fun resume() {
        startAR()
    }

    external fun isARStarted(): Boolean
    external fun cameraPerformAutoFocus()
    external fun cameraRestoreAutoFocus()
    external fun initRendering()
    external fun configureRendering(width: Int, height: Int, orientation: Int, rotation: Int): Boolean
    external fun renderFrame(): Boolean

    private external fun initAR(activity: Activity, fileName: String, targetNames: Array<String>)
    private external fun startAR(): Boolean
    private external fun stopAR()
    private external fun deinitAR()

    /// Called from native code once the engine and observers are created, on the [start] background thread.
    @Suppress("unused")
    private fun initDone() {
        val started = startAR()
        mainHandler.post { startedCallback?.invoke(started) }
    }

    /// Called from native code on a Vuforia error message.
    @Suppress("unused")
    private fun presentError(error: String) {
        Log.e(TAG, error)
    }

    /// Called from native code every frame, on the GL render thread.
    @Suppress("unused")
    private fun onDetection(targetName: String?) {
        mainHandler.post { callback(targetName) }
    }

    companion object {
        private const val TAG = "VuforiaWorker"

        init {
            System.loadLibrary("banknotesreader")
        }
    }
}