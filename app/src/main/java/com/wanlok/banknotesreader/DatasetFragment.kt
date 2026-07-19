package com.wanlok.banknotesreader

import android.os.Bundle
import android.view.View
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.appbar.MaterialToolbar

/// Matches iOS's VuforiaDatasetViewController: lists the dataset's targets (name/size) with
/// a toolbar "Sync" action that downloads fresh ones from https://wanlok.github.io/, falling
/// back to an empty-state placeholder like iOS's PlaceholderView when there's nothing to show.
class DatasetFragment : Fragment(R.layout.fragment_dataset) {

    private var syncing = false
    private val adapter = DatasetAdapter()

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

        val targetRecyclerView = view.findViewById<RecyclerView>(R.id.targetRecyclerView)
        targetRecyclerView.layoutManager = LinearLayoutManager(requireContext())
        targetRecyclerView.adapter = adapter

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
        adapter.submitList(targets)

        requireView().findViewById<View>(R.id.targetRecyclerView).visibility = if (targets.isEmpty()) View.GONE else View.VISIBLE
        requireView().findViewById<View>(R.id.emptyState).visibility = if (targets.isEmpty()) View.VISIBLE else View.GONE
    }
}
