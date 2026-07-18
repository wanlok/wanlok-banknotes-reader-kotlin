package com.wanlok.banknotesreader

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment

/// Hardcoded-dataset smoke test for the Vuforia detect flow: bundles the real
/// banknotesReader.xml/.dat as assets (rather than the eventual runtime download/sync)
/// and wires VuforiaView/VuforiaWorker together end-to-end once CAMERA is granted.
/// Matches iOS's Camera tab, scoped to Vuforia only - no detection-method switching.
class CameraFragment : Fragment(R.layout.fragment_camera) {

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

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View {
        return inflater.inflate(R.layout.fragment_camera, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        detectionLabel = view.findViewById(R.id.detectionLabel)

        if (ContextCompat.checkSelfPermission(requireContext(), Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            cameraPermissionGranted = true
        } else {
            requestCameraPermission.launch(Manifest.permission.CAMERA)
        }
    }

    private fun startVuforia() {
        val datasetPath = DatasetAssets.copyIfNeeded(requireContext())

        val worker = VuforiaWorker(requireActivity()) { targetName ->
            detectionLabel.text = targetName ?: getString(R.string.point_camera_at_banknote)
        }
        vuforiaWorker = worker

        val view = VuforiaView(requireContext(), worker)
        vuforiaView = view
        requireView().findViewById<FrameLayout>(R.id.vuforiaContainer).addView(view)

        worker.start(datasetPath, DatasetAssets.TARGET_NAMES) { started ->
            if (!started) {
                detectionLabel.text = getString(R.string.vuforia_start_failed)
            }
        }
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

    /// Fired by MainActivity's show()/hide() tab switching (not a real Activity-level
    /// pause/resume) - reuses the same pause()/resume() pair so the camera isn't held open
    /// while the Settings tab is showing, without tearing down/reinitializing the Vuforia
    /// engine on every tab switch the way replace() would.
    override fun onHiddenChanged(hidden: Boolean) {
        super.onHiddenChanged(hidden)
        if (vuforiaView == null) {
            return
        }
        if (hidden) {
            vuforiaWorker?.pause()
        } else {
            vuforiaWorker?.resume()
        }
    }

    override fun onDestroyView() {
        vuforiaWorker?.stop()
        vuforiaView = null
        vuforiaWorker = null
        super.onDestroyView()
    }
}