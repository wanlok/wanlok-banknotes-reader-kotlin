package com.wanlok.banknotesreader

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Xml
import org.xmlpull.v1.XmlPullParser
import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL

/// Shared access to the Vuforia dataset (downloaded to internal storage as
/// banknotesReader.{xml,dat}), used by both CameraFragment (to start Vuforia) and the
/// Dataset settings screen (to display what's currently loaded). Mirrors iOS's
/// getVuforiaDatasetFilePaths.swift/NetworkViewController.downloadFiles.
object DatasetAssets {

    private const val DATASET_NAME = "banknotesReader"
    private const val BASE_URL = "https://wanlok.github.io"

    data class Target(val name: String, val size: String)

    /// Absolute path (without extension) that AppController::createObservers expects as fileName.
    fun datasetPath(context: Context): String = File(context.filesDir, DATASET_NAME).absolutePath

    private fun xmlFile(context: Context) = File(context.filesDir, "$DATASET_NAME.xml")
    private fun datFile(context: Context) = File(context.filesDir, "$DATASET_NAME.dat")

    /// Downloads banknotesReader.xml/.dat from https://wanlok.github.io/ into app storage,
    /// overwriting whatever's already there. Runs off the main thread; callback fires on it.
    fun sync(context: Context, callback: (Boolean) -> Unit) {
        val mainHandler = Handler(Looper.getMainLooper())
        Thread {
            val success = try {
                download("$BASE_URL/$DATASET_NAME.xml", xmlFile(context))
                download("$BASE_URL/$DATASET_NAME.dat", datFile(context))
                true
            } catch (e: IOException) {
                false
            }
            mainHandler.post { callback(success) }
        }.start()
    }

    private fun download(urlString: String, destination: File) {
        val connection = URL(urlString).openConnection() as HttpURLConnection
        try {
            connection.inputStream.use { input ->
                destination.outputStream().use { output -> input.copyTo(output) }
            }
        } finally {
            connection.disconnect()
        }
    }

    /// Parses ImageTarget name/size attributes out of the dataset XML, for both the target
    /// names Vuforia detects against and the Dataset settings screen's display list. Returns
    /// an empty list if the dataset hasn't been synced yet, rather than downloading it -
    /// that's sync()'s job.
    fun parseTargets(context: Context): List<Target> {
        val file = xmlFile(context)
        if (!file.exists()) {
            return emptyList()
        }

        val targets = mutableListOf<Target>()
        val parser = Xml.newPullParser()
        file.inputStream().use { input ->
            parser.setInput(input, null)
            var eventType = parser.eventType
            while (eventType != XmlPullParser.END_DOCUMENT) {
                if (eventType == XmlPullParser.START_TAG && parser.name == "ImageTarget") {
                    val name = parser.getAttributeValue(null, "name")
                    val size = parser.getAttributeValue(null, "size")
                    if (name != null) {
                        targets.add(Target(name, size ?: ""))
                    }
                }
                eventType = parser.next()
            }
        }
        return targets
    }
}
