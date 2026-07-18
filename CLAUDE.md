# banknotes-reader (Android)

Android port of the iOS app at `/Volumes/A/Files/Projects/wanlok-banknotes-reader-swift`
(`banknotes-reader`) — an accessibility app for visually impaired users that identifies
banknotes via camera-based image recognition and announces the currency/amount via
text-to-speech.

The port is scoped to **Vuforia only, first** (not ARCore, not a Vision-based approach,
not the Dummy placeholder iOS also has). Settings screens, language switching, other
detection methods, and full Room/dataset caching parity are deferred until the Vuforia
detect → TTS flow works end-to-end.

## Reference material

- iOS source: `/Volumes/A/Files/Projects/wanlok-banknotes-reader-swift/banknotes-reader/`
  — the working reference for behavior/UX to match. Relevant Vuforia files under
  `Features/Detections/Vuforia/`.
- Downloaded PTC packages (not part of any repo, but useful references):
  - `~/Downloads/vuforia-sdk-android-11-4-4/` — the Vuforia Android SDK (headers, jar, `.so`)
  - `~/Downloads/vuforia-sample-11-4-4/` — PTC's cross-platform + Android sample app, source
    of `GLESRenderer`/`GLESUtils`/`Shaders.h`/`VuforiaWrapper.cpp` before adaptation

## Current state

**Done — native Vuforia integration, builds clean (`./gradlew :app:externalNativeBuildDebug`):**

- `CrossPlatform/AppController.{h,cpp}` — PTC's cross-platform Vuforia core, customized on
  iOS to support a runtime-downloaded multi-target dataset (banknote denominations) instead
  of a single hardcoded demo target: `initAR` takes `fileName`/`targetNames`/`targetCount`,
  and a `DetectionCallback` reports which target (if any) is on screen each frame. Ported
  from the iOS repo's copy, with dead code (leftover commented-out Model Target / single-
  hardcoded-target logic) removed and `mDetectionCallback` moved to private members where it
  belongs. **Both repos carry an identical, cleaned copy of this file** — if it needs further
  changes, update both.
- `app/libs/VuforiaEngine.jar`, `app/src/main/jniLibs/arm64-v8a/libVuforiaEngine.so`,
  `app/src/main/cpp/vuforia-sdk/include/VuforiaEngine/` — copied in from the downloaded SDK
  package (arm64-v8a only — VuforiaEngine doesn't ship other ABIs). Everything is copied
  into the repo rather than referenced from `~/Downloads` (portability) — the sample
  project's `VUFORIA_ENGINE_PATH` external-reference approach was deliberately avoided.
- `app/src/main/cpp/GLESRenderer.{h,cpp}` — adapted from the sample: only renders the video
  background and a bounding-box overlay on a detected Image Target. The sample's Model
  Target / Astronaut / Lander demo-content code was **removed, not just left unused** —
  it was actually a landmine (`renderImageTarget` unconditionally rendered an "Astronaut"
  3D model using vertex data that would be uninitialized/empty without bundling `.obj`
  assets we don't want). This mirrors the iOS `MetalRenderer.swift`, which already has the
  equivalent astronaut/axis rendering commented out — Android now matches that behavior
  intentionally, not by accident.
- `app/src/main/cpp/GLESUtils.{h,cpp}`, `Shaders.h` — copied from the sample unmodified.
- `app/src/main/cpp/VuforiaWrapper.cpp` — JNI bridge (Android's equivalent of iOS's
  `VuforiaWrapper.mm`). Adapted from the sample: ARCore support entirely stripped out (not
  needed, avoids a Google Play Services AR dependency), `initAR` updated to marshal
  `fileName`/`targetNames` from Kotlin and wire the `detectionCallback`. JNI symbol names
  target the Kotlin class `com.wanlok.banknotesreader.VuforiaWorker` below.
- `app/src/main/cpp/CMakeLists.txt` — builds `AppController.cpp` + the four files above
  against the in-repo copies only, links `libVuforiaEngine.so` as an `IMPORTED` target.
- `app/build.gradle.kts` — NDK/CMake wired up (`abiFilters = ["arm64-v8a"]`,
  `externalNativeBuild`), jar dependency added.
- `app/src/main/java/com/wanlok/banknotesreader/VuforiaWorker.kt` — Kotlin counterpart to
  `VuforiaWrapper.cpp`'s JNI bridge, mirrors iOS's `VuforiaWorker.swift`. `external fun`
  declarations for the full native surface (`initAR`/`startAR`/`stopAR`/`deinitAR`/
  `isARStarted`/`cameraPerformAutoFocus`/`cameraRestoreAutoFocus`/`initRendering`/
  `configureRendering`/`renderFrame`) plus the three callback methods native code calls
  into (`presentError`/`initDone`/`onDetection`). Deliberately takes already-resolved
  `fileName`/`targetNames` as parameters to `start()` rather than reaching into dataset
  files itself like iOS's version does — dataset sync is still a separate unstarted piece
  below, so the worker stays decoupled from it. `initAR` does blocking engine/observer
  creation, so `start()` runs it on a background `Thread`; `initDone()` (called back on
  that same thread) calls `startAR()` and posts the result to the main thread;
  `onDetection` fires every frame from the GL render thread and also posts to main.
  Both `./gradlew :app:compileDebugKotlin` and `:app:externalNativeBuildDebug` pass.
  **✅ Exercised at runtime and confirmed working** (see `MainActivity.kt` below).
  ⚠️ Latent gap carried over from `VuforiaWrapper.cpp`, not introduced here: its
  `errorMessageCallback`/`vuforiaEngineErrorCallback` do `JavaVM::GetEnv` without
  `AttachCurrentThread`, so if Vuforia ever invokes them from an SDK-internal thread
  (not one that called into JNI itself), the callback silently no-ops. `initDone` and
  `onDetection` are unaffected since they always fire on a thread that's already inside
  a JNI call (the `start()` background thread / the GL render thread).
- `app/src/main/java/com/wanlok/banknotesreader/VuforiaView.kt` — `GLSurfaceView`-based
  camera view, Android's equivalent of iOS's `VuforiaView.swift` (`CADisplayLink` +
  `CAMetalLayer`). Implements `GLSurfaceView.Renderer`: `onSurfaceCreated` →
  `worker.initRendering()`; `onSurfaceChanged` stores width/height and flags a re-configure;
  `onDrawFrame` re-runs `worker.configureRendering(width, height,
  resources.configuration.orientation, display.rotation)` whenever the surface changed or
  `display.rotation` differs from last frame (rotation handling from the sample app's
  `VuforiaActivity.kt`, which pairs `Configuration.orientation` with `Surface.ROTATION_*` —
  same convention `VuforiaWrapper.cpp`'s `configureRendering` already expects), then calls
  `worker.renderFrame()`. Takes the `VuforiaWorker` via constructor injection rather than
  mirroring iOS's split (there, `VuforiaWorker` holds a reference to `VuforiaView` to push
  a `mVuforiaStarted` flag into it) — Android's `worker.isARStarted()` is a cheap native
  query, so `onDrawFrame` just asks it directly each frame instead of syncing a duplicate
  Kotlin-side flag. `./gradlew :app:compileDebugKotlin` passes.
  **✅ Exercised at runtime and confirmed working** (see `MainActivity.kt` below).
- `app/src/main/java/com/wanlok/banknotesreader/MainActivity.kt` — wires `VuforiaView`/
  `VuforiaWorker` together for the first time, as a **hardcoded-dataset smoke test**, not
  the real dataset sync / detection UI flow (both still below). Requests `CAMERA` at
  runtime via `ActivityResultContracts.RequestPermission`; on grant, copies the real
  production dataset (bundled as `app/src/main/assets/banknotesReader.{xml,dat}`, fetched
  from `https://wanlok.github.io/` — not synthetic data, Vuforia databases are trained
  binaries that can't be fabricated) to `filesDir` on first run and calls
  `worker.start()` with an **absolute filesystem path** (not a bare relative name) and a
  **hardcoded** `targetNames` array (`aud_100`/`aud_50`/`aud_20`, read once from the
  dataset XML) rather than parsing it — XML parsing is still the dataset-sync item below.
  The absolute-path choice was deliberate: `AppController::createObservers` passes
  `databasePath` straight to `vuEngineCreateImageTargetObserver` with no documented
  relative-path resolution rule for Android in the Vuforia headers or PTC's sample (whose
  own `"StonesAndChips.xml"` relative-path usage isn't reachable/verifiable), so an
  absolute path sidesteps that ambiguity entirely — and it's what real dataset sync will
  need anyway (downloaded files, not bundled assets). Detection result flows straight to
  a `TextView` (`detectionLabel`) with no TTS. The actual `worker.start()` call is
  deferred to `onResume()` (guarded by a `cameraPermissionGranted` flag set in `onCreate`/
  the permission callback) rather than fired straight from `onCreate`/the permission
  callback, to keep camera-touching work off the activity's initial creation path.
  **✅ Exercised at runtime and confirmed working** on a physical device (Xiaomi/MIUI,
  Android 13) — live camera passthrough renders, detection label updates.
  `onPause()`/`onResume()` call `VuforiaWorker.pause()`/`resume()` (thin wrappers around
  the already-private `stopAR()`/`startAR()` externs) — without this, backgrounding the
  app left the camera capture session dangling once Android reclaimed the camera device,
  and detection silently stopped working even after returning to the foreground. Matches
  PTC's own sample (`VuforiaActivity.kt`'s `onPause`/`onResume` call the same pair);
  `AppController::startAR`/`stopAR` are safe to call repeatedly since they guard on
  `vuEngineIsRunning()` and don't touch `mEngine`/observers, so no re-init is needed.
- `app/src/main/AndroidManifest.xml` — `CAMERA` permission (+ camera/autofocus/GLES3
  `<uses-feature>` declarations), plus `INTERNET`/`ACCESS_NETWORK_STATE`/
  `HIGH_SAMPLING_RATE_SENSORS`. **The latter three were the actual fix for a misleading
  on-device failure**: `vuEngineCreate` kept returning `VU_ENGINE_CREATION_ERROR_PERMISSION_ERROR`
  → `AppController::initErrorToString` → *"Vuforia cannot initialize because access to the
  camera was denied"* — which reads like a `CAMERA` runtime-permission problem, but
  `CAMERA` was confirmed granted at every layer (`dumpsys package`, `appops get` showing
  `foreground` mode, MIUI's separate Security-app permission list) and a deliberate 1.9s
  startup delay ruled out a foreground-timing race too. The real cause: Vuforia needs
  `INTERNET`/`ACCESS_NETWORK_STATE` to validate the license key against PTC's servers, and
  `HIGH_SAMPLING_RATE_SENSORS` for its device tracker on Android 12+ — none of which were
  declared. Found by diffing against PTC's own sample manifest
  (`~/Downloads/vuforia-sample-11-4-4/Android/app/src/main/AndroidManifest.xml`), which
  comments all three as "Required by Vuforia." **If a future permission-flavored Vuforia
  init error shows up again, check this manifest diff first before chasing OS/MIUI
  permission state.**
- `app/src/main/assets/banknotesReader.{xml,dat}` — the real hosted dataset, bundled for
  the hardcoded smoke test above. **Temporary** — remove once dataset sync (below) can
  download it at runtime instead.

**Not started yet:**

- Dataset sync: download `banknotesReader.xml`/`.dat` from `https://wanlok.github.io/` into
  app storage at runtime (rather than the bundled-asset stand-in above), parse `ImageTarget`
  `name` attributes via `XmlPullParser` (Android equivalent of iOS's
  `getVuforiaDatasetFilePaths.swift`/`getXMLAttributeValues.swift`) instead of the hardcoded
  `aud_100`/`aud_50`/`aud_20` array in `MainActivity.kt`.
- Detection UI flow: a real amount overlay (equivalent of `AmountView.swift`/
  `AmountDetectionViewController.swift`) replacing `MainActivity`'s plain `detectionLabel`
  `TextView`, and `android.speech.tts.TextToSpeech` integration respecting a "voice enabled"
  preference and TalkBack state (`AccessibilityManager.isTouchExplorationEnabled()`).

## Conventions carried over from the iOS app

- Target names are encoded as `"currency_amount"`, e.g. `"aud_50"` (split on `_`).
- `AppController` target is always `IMAGE_TARGET_ID` (`0`) — Model Targets are unused.
- Dataset file base name: `banknotesReader` (`.xml`/`.dat`, hosted at
  `https://wanlok.github.io/banknotesReader.{xml,dat}`).

## Working with this user

- Prefer direct text recommendations over the `AskUserQuestion` tool for setup/scoping
  decisions — they've rejected that tool before and prefer to redirect a stated
  recommendation instead.
- The user wrote the original iOS Vuforia customizations themselves and is open to (and
  wants) code review/cleanup before porting patterns forward, rather than blind ports.
