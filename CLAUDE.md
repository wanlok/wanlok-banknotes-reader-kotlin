# banknotes-reader (Android)

Android port of the iOS app at `/Volumes/A/Files/Projects/wanlok-banknotes-reader-swift`
(`banknotes-reader`) — an accessibility app for visually impaired users that identifies
banknotes via camera-based image recognition and announces the currency/amount via
text-to-speech.

The port is scoped to **Vuforia only, first** (not ARCore, not a Vision-based approach,
not the Dummy placeholder iOS also has). Language switching and full Room/dataset caching
parity are deferred until the Vuforia detect → TTS flow works end-to-end. Settings now has
a minimal presence (see bottom nav bar below) — just enough to mirror iOS's shape
(Detection Method / Dataset rows), not full parity (no method switching, since only Vuforia
is ported; no language settings).

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
  `stop()` also calls `mainHandler.removeCallbacksAndMessages(null)` — found via an actual
  on-device crash on rotation (Activity has no `configChanges` override, so it's fully
  destroyed/recreated): an `onDetection`/`initDone` post already queued right before
  `CameraFragment.onDestroyView()` → `worker.stop()` would otherwise still run afterward
  against the by-then-detached fragment (`getString()` → `requireContext()` throws
  `IllegalStateException`). `CameraFragment.kt`'s two `VuforiaWorker` UI callbacks also
  guard with `isAdded` as a second layer, for the case where a callback is already mid-flight
  on the main looper when teardown starts.
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
- `app/src/main/java/com/wanlok/banknotesreader/MainActivity.kt` — now just the bottom-nav
  shell (see below); the Vuforia wiring that used to live here directly moved to
  `CameraFragment.kt` when the bottom nav bar was added.
- **Bottom navigation bar** (Camera / Settings tabs), mirroring iOS's `UITabBarController`
  (`SceneDelegate.swift`) — deliberately trimmed to what this port actually supports rather
  than a literal port: iOS's Camera tab dynamically swaps between ARKit/Vuforia/Dummy
  detection controllers (default ARKit) and Settings has a full method-picker; Android only
  ever has Vuforia, so Camera is fixed and Settings is scoped to just the Detection Method
  (single, always-checkmarked "Vuforia" row) and Dataset rows, dropping ARKit/Dummy/Vision
  and language settings entirely. **✅ Exercised at runtime and confirmed working**
  end-to-end (tab switching, back-stack, Vuforia pause/resume across tab switches) on a
  physical device.
  - `MainActivity.kt` — hosts a `BottomNavigationView` + a `fragmentContainer`. Adds
    `CameraFragment` and `SettingsFragment` once (`savedInstanceState == null`) and toggles
    between them via `show()`/`hide()` (never `replace()`), so switching tabs doesn't tear
    down/reinitialize the Vuforia session. `showTab()` also calls
    `setPrimaryNavigationFragment()` on the active tab so the system back button correctly
    delegates to whichever tab's own back stack is deepest (needed for Settings' internal
    push navigation below). Its `enableEdgeToEdge()` insets listener only pads `main` for
    `systemBars.left/top/right`, deliberately excluding bottom — `bottomNavigation` consumes
    its own bottom inset (Material's real, working default behavior for edge-to-edge; the
    `paddingBottomSystemWindowInsets` XML attribute some Material docs/snippets reference
    turned out to be a no-op in this Material version, confirmed by decompiling
    `material-1.10.0.aar` — nothing in it actually reads that attribute). Padding both root
    and the nav view for the same bottom inset was double-counting it, showing as dead white
    space below the Camera/Settings labels; letting the nav view own it is also what makes
    the bar's background correctly extend under 3-button/gesture nav rather than stopping
    short of it. Left/right padding stays on `main` for landscape (side nav bar / cutouts);
    currently unverified in practice since the app has no orientation lock and no landscape
    layout pass has been done — **a real decision still pending**: lock to portrait (this is
    a one-handed camera-pointing UX, arguably the better fit, and would let this insets
    handling drop the left/right case entirely) vs. actually support landscape.
  - `CameraFragment.kt` — the Vuforia flow, moved verbatim from the old `MainActivity.kt`
    (permission handling, `VuforiaWorker`/`VuforiaView` construction, `onPause`/`onResume`
    calling `pause()`/`resume()`). Adds one thing: `onHiddenChanged(hidden)` also calls
    `vuforiaWorker.pause()`/`resume()` when `MainActivity` shows/hides this tab — reuses the
    exact same pause/resume pair built for Activity-level backgrounding rather than
    reinitializing Vuforia on every tab switch (which naive `replace()`-based tab switching
    would otherwise force).
  - `res/values/colors.xml` — all color values used by layouts route through here now
    (`black`/`white`/`detection_label_background`); no more raw hex in layout XML. Doesn't
    reach into `GLESRenderer.cpp`'s detection-highlight green — that's raw GL floats, a
    separate system with no Android resource equivalent.
  - `DatasetAssets.kt` — new shared object extracted from the old `MainActivity.kt`'s
    dataset-asset-copying logic, now used by both `CameraFragment` and `DatasetFragment`.
    `TARGET_NAMES` stays **hardcoded** (not parsed) — still intentionally decoupled from the
    XML, since that's the separate dataset-sync work below. Also adds `parseTargets()`
    (`XmlPullParser` over `ImageTarget` name/size attributes) — used **only** to populate
    the Dataset settings screen, not to drive detection; this is the first real XML parsing
    in the repo, and a natural head start on the dataset-sync work below.
  - `SettingsFragment.kt` — nav host for the Settings tab, matching iOS's
    `UINavigationController` wrapping `SettingLandingViewController`: owns its own child
    `FragmentManager` so its push navigation (landing → Detection Method / Dataset) stays
    isolated from `MainActivity`'s top-level tab-switching `FragmentManager` — pushing a
    sub-screen here must not tear down the (possibly hidden) `CameraFragment` living in that
    other `FragmentManager`. `SettingsLandingFragment.kt` is the actual two-row list.
  - `DetectionMethodFragment.kt` — matches iOS's `DetectionMethodViewController`, minus the
    list: always a single checkmarked "Vuforia" row. Kept as a real screen (not dropped)
    purely to keep the Settings shape consistent with iOS.
  - `DatasetFragment.kt` — matches iOS's `VuforiaDatasetViewController`: lists real parsed
    targets (name/size) via `DatasetAssets.parseTargets()`, with a toolbar "Sync" action and
    an empty-state placeholder. **"Sync" is a placeholder** — it just re-copies the bundled
    assets rather than downloading fresh ones from `https://wanlok.github.io/`; real network
    sync is still the dataset-sync item below. Otherwise fully functional today.
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
  app storage at runtime (rather than the bundled-asset stand-in in `DatasetAssets.kt`),
  parse `ImageTarget` `name` attributes via `XmlPullParser` (already exists as
  `DatasetAssets.parseTargets()`, currently only used to populate the Dataset settings
  screen — Android equivalent of iOS's `getVuforiaDatasetFilePaths.swift`/
  `getXMLAttributeValues.swift`) instead of the hardcoded `TARGET_NAMES` array. Also needs
  `DatasetFragment`'s "Sync" action wired to the real download instead of its current
  re-copy-bundled-assets placeholder.
- Detection UI flow: a real amount overlay (equivalent of `AmountView.swift`/
  `AmountDetectionViewController.swift`) replacing `CameraFragment`'s plain `detectionLabel`
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
