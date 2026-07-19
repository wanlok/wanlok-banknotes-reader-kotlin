package com.wanlok.banknotesreader

import android.view.LayoutInflater
import android.view.ViewGroup
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView

class DatasetAdapter : RecyclerView.Adapter<DatasetAdapter.ViewHolder>() {

    private var targets: List<DatasetAssets.Target> = emptyList()

    fun submitList(targets: List<DatasetAssets.Target>) {
        this.targets = targets
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_dataset_target, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val target = targets[position]
        holder.name.text = target.name
        holder.size.text = target.size
    }

    override fun getItemCount(): Int = targets.size

    class ViewHolder(view: android.view.View) : RecyclerView.ViewHolder(view) {
        val name: TextView = view.findViewById(R.id.targetName)
        val size: TextView = view.findViewById(R.id.targetSize)
    }
}
