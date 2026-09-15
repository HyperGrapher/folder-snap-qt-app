# FolderSnap

A Windows 10/11 FolderSnap interface foundation built with C++20 and Qt Quick. Four retained pages,
custom controls, a native frameless window, and one GPU shader explore color and
motion. The UI still uses local mock state and resets when closed. The first backend
milestone adds validated snapshot/configuration contracts, Windows path safety,
and ordered exclusion rules independently of the UI.

## Backend foundation

`folder_snap_core` depends only on Qt Core, not QML. It provides schema-v2 JSON
encoding/decoding, exact 64-bit metadata and nanosecond UTC timestamps, validated
paths/IDs, and exclusion matching with protected application-data subtrees.
See [core contracts](docs/260915-Core_Data_Contracts.md) for behavior and limits.

Configuration and the lightweight history index now save atomically in the user's
local application-data folder. Invalid saved JSON is preserved in a `corrupt/`
subfolder before FolderSnap returns safe defaults. This milestone does not yet scan
folders, store snapshot payloads, or connect live data to the interface.

Run only the new core tests after building:

```powershell
ctest --test-dir build --output-on-failure -R '^(domain|paths|ignore|storage)$'
```

## Build and run

Tested toolchain: Qt **6.11.1 MinGW 64-bit**, its **MinGW 13.1** compiler, CMake,
and Ninja. Install the Qt Quick, Quick Controls, Shader Tools, and test components.
Use the compiler shipped with that Qt kit; do not mix MinGW and MSVC libraries.

From the project root in PowerShell:

```powershell
$qtRoot = 'C:\Qt\6.11.1\mingw_64'
$compilerRoot = 'C:\Qt\Tools\mingw1310_64'
$vcpkgRoot = 'C:\Users\burak\vcpkg'
$triplet = 'x64-mingw-dynamic'
$env:PATH = "$compilerRoot\bin;$qtRoot\bin;$env:PATH"

& "$vcpkgRoot\vcpkg.exe" install --triplet $triplet `
    "--x-install-root=$PWD\build\vcpkg_installed"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$qtRoot" `
    "-DCMAKE_CXX_COMPILER=$compilerRoot\bin\g++.exe" `
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot\scripts\buildsystems\vcpkg.cmake" `
    "-DVCPKG_INSTALLED_DIR=$PWD\build\vcpkg_installed" `
    "-DVCPKG_TARGET_TRIPLET=$triplet" `
    -DFOLDERSNAP_INSTALL_DEPENDENCIES=OFF -DBUILD_TESTING=ON
cmake --build build -j 4
ctest --test-dir build --output-on-failure
& .\build\FolderSnap.exe
```

Only `build/` is used for build products. A missing Vulkan-headers notice is not
an error: Windows uses Qt's Direct3D 11 backend by default. Shader Tools compiles
the fragment shader to the embedded `.qsb` resource during the build.

## Deploy a standalone folder

Run after the build, in the same PowerShell environment:

```powershell
New-Item -ItemType Directory -Force build/deploy | Out-Null
Copy-Item build/FolderSnap.exe build/deploy/FolderSnap.exe
& "$qtRoot/bin/windeployqt.exe" --release --compiler-runtime --no-translations `
    --qmldir src/qml --dir build/deploy build/deploy/FolderSnap.exe
& .\build\deploy\FolderSnap.exe
```

Keep the entire deployment folder together; the EXE alone is insufficient.
Deployment includes Qt's runtime libraries and QML plugins. No installer is provided.

## What to try

- Switch pages quickly; selections retarget without queuing navigation.
- Add a sample folder, edit folder preferences, and take a simulated snapshot.
- In Compare, choose two timeline moments, run the comparison, search/filter the tree,
  and review Added items through the two-stage cleanup preview.
- In Settings, use **Explore the preview** to inspect empty, missing-payload, warning,
  failure, and large-comparison states.
- Use Tab/Shift+Tab, Space, and Enter to operate controls. Focus has a visible outline.
- Turn on Reduced motion or turn off Background motion in Settings.
- Resize from every edge, double-click empty title space, and maximize/restore.

## Where to change the design

- `src/qml/Theme.qml`: palette, font, spacing, corners, timing, easing, shell dimensions.
- `src/qml/controls/`: styled reusable Qt Quick Controls and display widgets.
- `src/qml/pages/`: four independent, scrollable page compositions.
- `src/qml/navigation/`: stable sidebar and retained-page transition host.
- `src/qml/effects/AmbientBackground.qml` and `resources/shaders/ambient.frag`:
  palette interpolation and the four-blob fragment shader.
- `src/qml/preview/UiPreviewState.qml`: deterministic in-memory product scenarios.
- `src/AppState.*`: navigation and motion preferences shared with C++ tests.
- `src/WindowsWindowController.*`: native frame, hit testing, work-area sizing,
  corner clipping, and window exposure. Keep Win32 APIs out of QML.

`Main.qml` owns an in-memory `UiPreviewState`. Child components receive it explicitly.
`MotionPolicy` combines user preferences with window visibility and exposure. Four
page instances are created once. During transitions all page input is disabled;
only the final selected page becomes enabled. Hidden pages retain their state
without decorative animation.

## Windows behavior and limitations

Windows 10 uses a real native rounded region, updated for size and DPI changes.
Windows owns a successfully applied region. Corners are square while maximized.
The default window is 1280 × 840, with a normal minimum of 960 × 680 logical pixels.
Initial and minimum dimensions are capped by the available screen area so high
display scaling does not force the maximized window over the taskbar. The sidebar's
decorative note is hidden when vertical space is limited; page content remains scrollable.
Region edges can be less smooth than compositor-rounded edges, and a native shadow
is not guaranteed.

Windows 11 uses an opaque, unmasked window and requests DWM rounding. Windows may
suppress rounding when snapped, maximized, or in some virtual/remote environments.
The DWM request is a hint, not a guarantee. Native failures are logged. The custom
maximize button does not implement the Windows 11 Snap Layout hover flyout.

The title area currently contains only three interactive window buttons. If you add
another interactive title control, extend the controller's caption exclusions.
Never mark an interactive control as caption space.

References: [Microsoft rounded-window guidance](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-rounded-corners),
[native region ownership](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowrgn),
[Qt ShaderEffect](https://doc.qt.io/qt-6/qml-qtquick-shadereffect.html).

## Verification

CTest runs state tests, offscreen QML tests, and a real desktop-window test. The
desktop test requires an unlocked interactive Windows session and a graphics backend.
For a headless environment, run only:

```powershell
ctest --test-dir build -R '^(appstate|ui)$' --output-on-failure
```

The desktop test keeps the overview screenshot at
`build/verification/scale-1.00/overview.png` and checks all main pages and dialogs
at the normal and minimum window sizes.

For four 60-second resource samples (longer than the ordinary CTest timeout):

```powershell
$env:FOLDERSNAP_MEASURE = '1'
& .\build\tst_window.exe performance
Remove-Item Env:FOLDERSNAP_MEASURE
```

Results are saved as `build/verification/scale-*/performance.json`. The harness
measures process CPU, memory, and presented-frame intervals. Frame intervals include
presentation scheduling; they are not GPU execution time. The process also contains
the test harness, so its memory is not an exact measurement of the standalone EXE.

Format C++ with the root `.clang-format` configuration and QML with Qt's `qmlformat`.
