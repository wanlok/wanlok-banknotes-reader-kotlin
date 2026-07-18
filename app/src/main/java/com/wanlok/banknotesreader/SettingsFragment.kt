package com.wanlok.banknotesreader

import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.fragment.app.commit

/// Nav host for the Settings tab, matching iOS's UINavigationController wrapping
/// SettingLandingViewController (SceneDelegate.swift): owns a child FragmentManager so its
/// own push navigation (landing -> Detection Method / Dataset) stays isolated from
/// MainActivity's top-level tab-switching FragmentManager - pushing a sub-screen here must
/// not tear down the (possibly hidden) CameraFragment living in that other FragmentManager.
class SettingsFragment : Fragment(R.layout.fragment_settings) {

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (childFragmentManager.findFragmentById(R.id.settingsContainer) == null) {
            childFragmentManager.commit {
                add(R.id.settingsContainer, SettingsLandingFragment())
            }
        }
    }
}
