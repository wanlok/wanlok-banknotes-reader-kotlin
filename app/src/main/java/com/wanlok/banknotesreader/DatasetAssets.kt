package com.wanlok.banknotesreader

import android.content.Context
import android.util.Xml
import org.xmlpull.v1.XmlPullParser
import java.io.File

/// Shared access to the bundled Vuforia dataset (app/src/main/assets/banknotesReader.{xml,dat})
/// used by both CameraFragment (to start Vuforia) and the Dataset settings screen (to display
/// what's currently loaded). Temporary - bundling real assets is a stand-in for runtime dataset
/// sync (still unbuilt), see CLAUDE.md.
object DatasetAssets {

    private const val DATASET_NAME = "banknotesReader"

    /// Hardcoded rather than parsed from the XML below, since detection's target list is
    /// still a separate, unbuilt piece of work (dataset sync) - kept in sync with
    /// banknotesReader.xml by hand for now.
    val TARGET_NAMES = arrayOf("aud_100", "aud_50", "aud_20")

    data class Target(val name: String, val size: String)

    /// Copies the bundled dataset assets to internal storage and returns the absolute
    /// path (without extension) that AppController::createObservers expects as fileName.
    fun copyIfNeeded(context: Context): String {
        val xmlFile = File(context.filesDir, "$DATASET_NAME.xml")
        val datFile = File(context.filesDir, "$DATASET_NAME.dat")
        if (!xmlFile.exists()) {
            context.assets.open("$DATASET_NAME.xml").use { it.copyTo(xmlFile.outputStream()) }
        }
        if (!datFile.exists()) {
            context.assets.open("$DATASET_NAME.dat").use { it.copyTo(datFile.outputStream()) }
        }
        return File(context.filesDir, DATASET_NAME).absolutePath
    }

    /// Parses ImageTarget name/size attributes out of the dataset XML, for display on the
    /// Dataset settings screen only - not used to drive detection (see TARGET_NAMES above).
    fun parseTargets(context: Context): List<Target> {
        copyIfNeeded(context)
        val xmlFile = File(context.filesDir, "$DATASET_NAME.xml")
        val targets = mutableListOf<Target>()

        val parser = Xml.newPullParser()
        xmlFile.inputStream().use { input ->
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
