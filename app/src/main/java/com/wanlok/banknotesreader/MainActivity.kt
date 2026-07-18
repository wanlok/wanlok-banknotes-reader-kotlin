package com.wanlok.banknotesreader

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.FrameLayout
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import java.io.File

/// Hardcoded-dataset smoke test for the Vuforia detect flow: bundles the real
/// banknotesReader.xml/.dat as assets (rather than the eventual runtime download/sync)
/// and wires VuforiaView/VuforiaWorker together end-to-end once CAMERA is granted.
class MainActivity : AppCompatActivity() {

    private lateinit var detectionLabel: TextView
    private var vuforiaView: VuforiaView? = null
    private var vuforiaWorker: VuforiaWorker? = null
    private var cameraPermissionGranted = false

    private val requestCameraPermission = registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        cameraPermissionGranted = granted
        if (granted) {
            if (vuforiaView == null) {
                startVuforia()
            }
        } else {
            detectionLabel.text = getString(R.string.camera_permission_required)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        detectionLabel = findViewById(R.id.detectionLabel)

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            cameraPermissionGranted = true
        } else {
            requestCameraPermission.launch(Manifest.permission.CAMERA)
        }
    }

    private fun startVuforia() {
        val datasetPath = copyDatasetAssetsIfNeeded()

        val worker = VuforiaWorker(this) { targetName ->
            detectionLabel.text = targetName ?: getString(R.string.point_camera_at_banknote)
        }
        vuforiaWorker = worker

        val view = VuforiaView(this, worker)
        vuforiaView = view
        findViewById<FrameLayout>(R.id.vuforiaContainer).addView(view)

        worker.start(datasetPath, TARGET_NAMES) { started ->
            if (!started) {
                detectionLabel.text = getString(R.string.vuforia_start_failed)
            }
        }
    }

    /// Copies the bundled dataset assets to internal storage and returns the absolute
    /// path (without extension) that AppController::createObservers expects as fileName.
    private fun copyDatasetAssetsIfNeeded(): String {
        val xmlFile = File(filesDir, "$DATASET_NAME.xml")
        val datFile = File(filesDir, "$DATASET_NAME.dat")
        if (!xmlFile.exists()) {
            assets.open("$DATASET_NAME.xml").use { it.copyTo(xmlFile.outputStream()) }
        }
        if (!datFile.exists()) {
            assets.open("$DATASET_NAME.dat").use { it.copyTo(datFile.outputStream()) }
        }
        return File(filesDir, DATASET_NAME).absolutePath
    }

    override fun onResume() {
        super.onResume()
        val view = vuforiaView
        if (view == null) {
            if (cameraPermissionGranted) {
                startVuforia()
            }
        } else {
            // Reacquire the camera before resuming the render thread: onPause() below
            // released it via VuforiaWorker.pause(), and Android may have reclaimed the
            // camera device entirely while backgrounded, so the AR session needs an
            // explicit restart rather than picking back up where it left off.
            vuforiaWorker?.resume()
            view.onResume()
        }
    }

    override fun onPause() {
        vuforiaView?.onPause()
        // Release the camera while backgrounded; without this the camera capture session
        // is left dangling once Android reclaims the device, and detection silently stops
        // working even after the app is foregrounded again.
        vuforiaWorker?.pause()
        super.onPause()
    }

    override fun onDestroy() {
        vuforiaWorker?.stop()
        super.onDestroy()
    }

    companion object {
        private const val DATASET_NAME = "banknotesReader"
        private val TARGET_NAMES = arrayOf("aud_100", "aud_50", "aud_20")
    }
}