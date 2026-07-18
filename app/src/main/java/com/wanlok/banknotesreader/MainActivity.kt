package com.wanlok.banknotesreader

import android.os.Bundle
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.commit
import com.google.android.material.bottomnavigation.BottomNavigationView

/// Bottom-nav shell mirroring iOS's UITabBarController (SceneDelegate.swift): a Camera tab
/// (CameraFragment, Vuforia only - no detection-method switching, unlike iOS) and a Settings
/// tab (SettingsFragment). Both top-level fragments are kept alive via show()/hide() rather
/// than replace(), so switching tabs doesn't tear down/reinitialize the Vuforia session -
/// CameraFragment.onHiddenChanged pauses/resumes it instead, reusing the same pause()/resume()
/// built for Activity-level backgrounding.
class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_main)
        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        if (savedInstanceState == null) {
            val settingsFragment = SettingsFragment()
            supportFragmentManager.commit {
                add(R.id.fragmentContainer, CameraFragment(), TAG_CAMERA)
                add(R.id.fragmentContainer, settingsFragment, TAG_SETTINGS)
                hide(settingsFragment)
            }
        }

        findViewById<BottomNavigationView>(R.id.bottomNavigation).setOnItemSelectedListener { item ->
            val tag = if (item.itemId == R.id.nav_camera) TAG_CAMERA else TAG_SETTINGS
            showTab(tag)
            true
        }
    }

    private fun showTab(tag: String) {
        val target = supportFragmentManager.findFragmentByTag(tag) ?: return
        supportFragmentManager.commit {
            for (fragment in listOf(TAG_CAMERA, TAG_SETTINGS).mapNotNull { supportFragmentManager.findFragmentByTag(it) }) {
                if (fragment === target) show(fragment) else hide(fragment)
            }
            setPrimaryNavigationFragment(target)
        }
    }

    companion object {
        private const val TAG_CAMERA = "camera"
        private const val TAG_SETTINGS = "settings"
    }
}
