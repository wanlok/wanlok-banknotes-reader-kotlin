package com.wanlok.banknotesreader

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import androidx.fragment.app.Fragment
import com.google.android.material.appbar.MaterialToolbar

/// Matches iOS's VuforiaDatasetViewController: lists the dataset's targets (name/size) with
/// a toolbar "Sync" action that downloads fresh ones from https://wanlok.github.io/, falling
/// back to an empty-state placeholder like iOS's PlaceholderView when there's nothing to show.
class DatasetFragment : Fragment(R.layout.fragment_dataset) {

    private var syncing = false

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val toolbar = view.findViewById<MaterialToolbar>(R.id.toolbar)
        toolbar.setNavigationOnClickListener { parentFragmentManager.popBackStack() }
        toolbar.setOnMenuItemClickListener { item ->
            if (item.itemId == R.id.action_sync) {
                sync()
                true
            } else {
                false
            }
        }

        reload()
    }

    private fun sync() {
        if (syncing) {
            return
        }
        syncing = true
        requireView().findViewById<MaterialToolbar>(R.id.toolbar).menu.findItem(R.id.action_sync).isEnabled = false
        DatasetAssets.sync(requireContext()) { success ->
            syncing = false
            if (!isAdded) {
                return@sync
            }
            requireView().findViewById<MaterialToolbar>(R.id.toolbar).menu.findItem(R.id.action_sync).isEnabled = true
            if (success) {
                reload()
                (activity as? MainActivity)?.onDatasetSynced()
            }
        }
    }

    private fun reload() {
        val targets = DatasetAssets.parseTargets(requireContext())

        val targetList = requireView().findViewById<LinearLayout>(R.id.targetList)
        targetList.removeAllViews()
        val inflater = LayoutInflater.from(requireContext())
        for (target in targets) {
            val row = inflater.inflate(R.layout.item_dataset_target, targetList, false)
            row.findViewById<TextView>(R.id.targetName).text = target.name
            row.findViewById<TextView>(R.id.targetSize).text = target.size
            targetList.addView(row)
        }

        requireView().findViewById<View>(R.id.targetListScroll).visibility = if (targets.isEmpty()) View.GONE else View.VISIBLE
        requireView().findViewById<View>(R.id.emptyState).visibility = if (targets.isEmpty()) View.VISIBLE else View.GONE
    }
}
