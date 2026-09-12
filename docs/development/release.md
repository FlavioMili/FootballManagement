# Building and releasing desktop packages

Three workflows in `.github/workflows/` build the game:

| Workflow      | Trigger                          | What it does                                                        |
|---------------|----------------------------------|---------------------------------------------------------------------|
| `ci.yml`      | push to any branch, pull request | Runs `build.yml` (packages kept 3 days) plus a Linux ASan/UBSan job |
| `release.yml` | tag `v*`, manual run             | Runs `build.yml`, then publishes a GitHub Release with checksums    |
| `build.yml`   | called by the two above          | Release build, headless tests, packaging, package smoke test        |

`deploy_docs.yml` (Doxygen site) is unrelated.

## Cutting a release

1. Make sure CI is green on the commit you want to ship, and run the full
   test suite locally (CI skips the slow labels, see below).
2. In `CHANGELOG.md`, replace "(unreleased)" in the heading of the version
   you are shipping with the release date and commit that change.
3. Tag it and push the tag:

   ```sh
   git tag -a v0.1.0 -m "Football Management 0.1.0"
   git push origin v0.1.0
   ```

4. `release.yml` builds every platform. When it finishes, the release
   `v0.1.0` appears on GitHub with:
   - `FootballManagement-0.1.0-linux-x86_64.tar.gz`
   - `FootballManagement-0.1.0-linux-x86_64.AppImage`
   - `FootballManagement-0.1.0-macos-arm64.zip`
   - `FootballManagement-0.1.0-windows-x86_64.zip` (only if the Windows job
     passed, see limitations)
   - `SHA256SUMS.txt` (check with `sha256sum -c SHA256SUMS.txt`)

   A tag with a hyphen (`v0.1.0-rc1`) becomes a pre-release. Release notes are
   generated from the commits and pull requests since the previous tag and can
   be edited on GitHub afterwards; paste the version's `CHANGELOG.md` section
   in. Re-running the workflow for an existing release replaces its files.

To try the pipeline without tagging, run **Release** manually from the Actions
tab: packages are uploaded as workflow artifacts (kept 30 days). Tick
*publish* to also create a **draft** release `v<version>` at that commit;
nothing is public until you publish the draft.

The version comes from the tag (without the `v`) and is passed to CMake as
`-DFM_VERSION=...`. It ends up in the package names and the macOS
`Info.plist`. Local builds default to `0.0.0`.

## What each job does

All jobs build `Release` with pinned FetchContent dependencies and cache the
fetched sources (`build/_deps/*-src`, `*-subbuild`). Linux and macOS also
cache compiler output with ccache. Packages use two extra CMake options so
they carry no library dependencies beyond the OS:
`-DSDLTTF_VENDORED=ON` (FreeType, HarfBuzz and PlutoSVG built in) and
`-DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=ON` (static SQLite from
`sjinks/sqlite3-cmake`). SDL3 is already built from source as a static
library.

Tests run headless (`SDL_VIDEO_DRIVER=dummy`, `SDL_AUDIO_DRIVER=dummy`) with
`ctest -LE "playtest|monkey|slow"`. That also skips the `adversarial` suite,
which is labelled `slow`. Run the full suite locally before a release.

**Linux** (`ubuntu-24.04`, GCC 14): links libstdc++ and libgcc statically,
builds and runs the tests, then runs `cpack` (tar.gz). It installs the
`game` component into an AppDir and turns it into an AppImage with a pinned
`linuxdeploy` release.

**macOS** (`macos-15`, Apple silicon): AppleClang's libc++ cannot build the
game. The code uses floating-point `std::from_chars` (player and training
repositories) and `std::jthread`/`std::stop_token` (thread pool), and libc++
only provides both without `-fexperimental-library` from LLVM 20. So the job
uses Homebrew `llvm` and links its `libc++.a`/`libc++abi.a` statically
(`-nostdlib++`). A check step fails the build if the game links anything
outside `/usr/lib` or `/System/Library`. CMake builds
`FootballManagement.app`, installs the assets into `Contents/Resources`,
ad-hoc signs the bundle as the last install step and zips it (`cpack` ZIP).
The job verifies the signature after extracting the zip.

**Windows** (`windows-2022`, Visual Studio generator with the ClangCL
toolset): builds the game and `fm_lab` with `BUILD_TESTING=OFF`, packs a
zip. The job is `continue-on-error`, so a failure never blocks CI or a
release.

**Smoke test** (all platforms, `packaging/smoke_test.sh`): extracts the
package to a temporary directory and starts the game headless from another
working directory for 10 s. On Linux and macOS it sends SIGTERM, which SDL
turns into a quit event. The script expects a clean exit and a
`Game data root:` log line pointing inside the package. On every platform it
checks that the log file appeared in the user-data directory and that no file
inside the package was created, changed or removed. You can run it locally
on any extracted package:

```sh
XDG_DATA_HOME=/tmp/fm-xdg packaging/smoke_test.sh <pkg> <pkg>/bin/FootballManagement \
  /tmp/fm-xdg/FlavioMili/FootballManagement
```

## Package layout and runtime paths

Installing and packaging live in `cmake/Packaging.cmake`. Only the `game`
install component is packaged, so the install rules of fetched dependencies
never add headers or static libraries.

| Platform | Executable                                   | Read-only game data                              |
|----------|----------------------------------------------|--------------------------------------------------|
| Linux    | `bin/FootballManagement`                     | `share/footballmanagement/assets`                |
| macOS    | `FootballManagement.app/Contents/MacOS/...`  | `FootballManagement.app/Contents/Resources/assets` |
| Windows  | `FootballManagement.exe`                     | `assets` next to the executable                  |

Linux packages also ship `share/applications/footballmanagement.desktop` and
a scalable icon. Licenses go to `share/doc/footballmanagement` on Linux and
to `licenses/` in the macOS and Windows zips: the game's `LICENSE` (GPL-3.0)
and `third_party/` notices for fmt, spdlog, nlohmann/json, Dear ImGui, SDL3,
SDL3_ttf, FreeType, HarfBuzz, PlutoSVG/PlutoVG, SQLite (public domain) and the
Roboto font (Apache-2.0).

`RuntimePaths::assetRoot()` finds the game data once per process. It uses
`FM_ASSET_ROOT` if set. Otherwise it takes the first directory holding
`assets/db/schema.sql` among `<exe>/../share/footballmanagement`,
`<exe>/../Resources` and `<exe>` (SDL's base path; inside a macOS bundle
that is already `Contents/Resources`). The fallback is the source checkout
baked in at configure time (`FM_SOURCE_DIR`), so development builds keep
working from `build/`. Code reads data through `AssetPaths::*()` in the
generated `global/paths.h`.

Writable files never go next to the executable. Saves, settings, logs,
`imgui.ini` and captures use `RuntimePaths::root()`, which is
`SDL_GetPrefPath("FlavioMili", "FootballManagement")`:

- Linux: `$XDG_DATA_HOME/FlavioMili/FootballManagement` (default
  `~/.local/share/...`)
- macOS: `~/Library/Application Support/FlavioMili/FootballManagement`
- Windows: `%APPDATA%\FlavioMili\FootballManagement`

## Building a package locally (Linux)

Use a separate build tree:

```sh
cmake -S . -B /tmp/fm-pkg -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DFM_VERSION=0.0.0-local -DSDLTTF_VENDORED=ON -DCMAKE_DISABLE_FIND_PACKAGE_SQLite3=ON \
  "-DCMAKE_EXE_LINKER_FLAGS=-static-libstdc++ -static-libgcc"
cmake --build /tmp/fm-pkg --parallel 2 --target FootballManagement
cpack --config /tmp/fm-pkg/CPackConfig.cmake -B /tmp/fm-pkg/dist
```

## Known limitations

- **macOS packages are not notarized or signed with a Developer ID** (only
  ad-hoc signed). The first launch is blocked by Gatekeeper. On macOS 15,
  open the app once, then go to *System Settings > Privacy & Security* and
  click *Open Anyway*. Alternatively run
  `xattr -dr com.apple.quarantine FootballManagement.app`. On older systems,
  right-click the app and choose *Open*. Real signing needs an Apple
  Developer account and `codesign`/`notarytool` secrets in the workflow.
- **macOS needs 15.0 or later on Apple silicon.** The static libc++ comes
  from Homebrew bottles built for the runner's macOS, so the deployment
  target is 15.0. There is no Intel or universal build: Homebrew's
  `libc++.a` is single-architecture.
- **The macOS package has no application icon** (it uses the generic one) and
  is a zip, not a DMG (`hdiutil` is unreliable on hosted runners).
- **Linux packages need glibc 2.39 or later** (the Ubuntu 24.04 baseline).
  libstdc++ is static, and SDL loads X11/Wayland/audio libraries at runtime
  from the host. The AppImage needs FUSE 2 to run directly. Without it, run
  it with `--appimage-extract-and-run`.
- **Windows is experimental.** The tests are not built on Windows because
  several test files use `unistd.h` and `setenv`/`unsetenv`. No Windows
  build has been checked yet. Possible problems include
  `std::filesystem::path` to `std::string` conversions (implicit only on
  POSIX), paths with non-ASCII characters, and warning flood: clang-cl
  treats the GCC-style `-Wall` as `-Weverything`. The GUI runs with the
  Windows subsystem (no console window), so check the log file in
  `%APPDATA%` for output.
- On macOS, every build (including development builds) produces
  `build/src/FootballManagement.app` instead of a plain executable. Pre-release
  versions such as `0.1.0-rc1` go into `CFBundleShortVersionString` as-is. That
  is not Apple's `x.y.z` format, which only matters for App Store submission.
- `linuxdeploy` is pinned to the `1-alpha-20251107-1` release. Homebrew
  `llvm` is not pinned, so the macOS compiler follows Homebrew's current LLVM.
