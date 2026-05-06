# sfc-gui

Desktop GUI for the SFC container format.
Built with Qt 6 Widgets — native look on macOS, Windows, and Linux.
Links directly against `libsfc_lib.a`; no CLI dependency.

## Features

- **Pack** — encode a file or directory into one or more `.sfc` files; set author, description, and other metadata; optional erasure-recovery chunks
- **Unpack** — decode one file or a set of split segments back to the original content; shows chunk availability grid
- **Info** — inspect metadata and structure of any `.sfc` file without decoding
- **Verify** — run BLAKE3 integrity checks across a batch of files and get a per-file pass/fail report

All encode/decode work runs on a background thread; the UI stays responsive with a progress bar.

## Prerequisites

| Tool | Version |
|------|---------|
| Qt 6 | 6.6+ (Widgets module) |
| C++23 compiler | Apple Clang 15+, GCC 13+, or MSVC 2022 |
| CMake | 3.25+ |
| libsfc_lib.a | built from `../sfc/` |

Build the sfc library first:

```sh
cd ../sfc && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

## Build

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build
```

**macOS** (Homebrew Qt):

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
./build/sfc-gui.app/Contents/MacOS/sfc-gui
```

**Windows** (vcpkg + MSVC):

```powershell
cmake -S . -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

**Linux** (system Qt or aqtinstall):

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/qt6
cmake --build build
```

`SFC_ROOT` defaults to `../sfc`. Override with `-DSFC_ROOT=/path/to/sfc` if your layout differs.

Qt apps must be built natively on the target OS — moc runs at build time on the host.
For producing release binaries across all three platforms without maintaining machines, use GitHub Actions (see `.github/workflows/`).

## Tests

```sh
cmake --build build --target sfc_gui_tests
./build/sfc_gui_tests
```

Tests cover `SfcWorker` (encode/decode/verify signal wiring), `MetadataEditor` (get/set round-trip), and `ChunkGridWidget` (edge cases without crashing).

