# SFC GUI

Desktop GUI for the SFC container format ([tnxbutno/sfc](https://github.com/tnxbutno/sfc)).
Built with Qt 6 Widgets — native look on macOS, Windows, and Linux.

## Features

- **Pack** — encode a file or directory into one or more `.sfc` files; set author, description, and other metadata; optional erasure-recovery chunks
- **Unpack** — decode one file or a set of split-transport segments back to the original content; shows chunk availability grid
- **Repair** — recover data from a corrupt or incomplete `.sfc` file; shows which chunks are missing and writes whatever content could be recovered
- **Info** — show metadata and structure of any `.sfc` file without decoding
- **Verify** — run BLAKE3 integrity checks across a batch of files and get a per-file pass/fail report

All encode/decode work runs on a background thread; the UI stays responsive with a progress bar.

## Prerequisites

| Tool | Version |
|------|---------|
| Qt 6 | 6.6+ (Widgets module) |
| C++23 compiler | Apple Clang 15+, GCC 13+, or MSVC 2022 |
| CMake | 3.25+ |
| zstd, brotli, lz4 | any recent version |
| sfc library | built from [tnxbutno/sfc](https://github.com/tnxbutno/sfc) |

## Build

### 1. Build the sfc library

Clone [tnxbutno/sfc](https://github.com/tnxbutno/sfc) as a sibling of this repo, then:

```sh
cd ../sfc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

If you cloned it elsewhere, pass `-DSFC_ROOT=/path/to/sfc` to the GUI's cmake step below.

### 2. Build the GUI

**macOS** (Homebrew):

```sh
brew install qt zstd brotli lz4
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
open build/sfc-gui.app
```

**Linux** (Ubuntu/Debian):

```sh
sudo apt-get install qt6-base-dev libzstd-dev libbrotli-dev liblz4-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/sfc-gui
```

**Windows** (Qt installer + vcpkg):

```powershell
vcpkg install zstd:x64-windows-static brotli:x64-windows-static lz4:x64-windows-static
cmake -S . -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.x/msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
```

## Tests

```sh
cmake --build build --target sfc_gui_tests
./build/sfc_gui_tests
```

Tests cover `SfcWorker` (encode/decode/verify/repair/split signal wiring), `MetadataEditor` (get/set round-trip), and `ChunkGridWidget` (edge cases without crashing).

## macOS: opening a downloaded build

CI builds are ad-hoc signed and will be blocked by Gatekeeper. If the app won't open even via right-click → Open, remove the quarantine attribute in Terminal:

```sh
xattr -dr com.apple.quarantine sfc-gui.app
```
