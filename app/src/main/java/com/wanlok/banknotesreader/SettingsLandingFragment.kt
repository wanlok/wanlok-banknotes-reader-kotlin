package com.wanlok.banknotesreader

import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.commit

/// Matches iOS's SettingLandingViewController: a two-row list (Detection Method, Dataset).
/// Android only ever supports Vuforia, so unlike iOS this never has a Dummy/ARKit case to
/// hide the Dataset row for (iOS omits it when the Dummy detection method is selected).
class SettingsLandingFragment : Fragment(R.layout.fragment_settings_landing) {

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        view.findViewById<TextView>(R.id.detectionMethodSubtitle).text = getString(R.string.detection_method_vuforia)

        view.findViewById<View>(R.id.detectionMethodRow).setOnClickListener {
            parentFragmentManager.commit {
                replace(R.id.settingsContainer, DetectionMethodFragment())
                addToBackStack(null)
            }
        }
        view.findViewById<View>(R.id.datasetRow).setOnClickListener {
            parentFragmentManager.commit {
                replace(R.id.settingsContainer, DatasetFragment())
                addToBackStack(null)
            }
        }
    }
}
