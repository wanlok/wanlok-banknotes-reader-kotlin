package com.wanlok.banknotesreader

import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import com.google.android.material.appbar.MaterialToolbar

/// Matches iOS's DetectionMethodViewController, minus the multi-method list: Android only
/// ever supports Vuforia, so this is always a single checkmarked row with nothing to choose -
/// kept as a real screen (rather than dropped, which iOS's own code would technically allow
/// for a single-item list) to keep the Settings shape consistent with iOS.
class DetectionMethodFragment : Fragment(R.layout.fragment_detection_method) {

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        view.findViewById<MaterialToolbar>(R.id.toolbar).setNavigationOnClickListener {
            parentFragmentManager.popBackStack()
        }
    }
}
