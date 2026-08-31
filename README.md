# GRFEditor-CPP

Cross-platform editor for Ragnarok Online **GRF / GPF / Thor** archives — a from-scratch C++20 reimplementation of Tokeiburu's Windows-only [GRF Editor](https://github.com/Tokeiburu/GRFEditor), built with SDL3, Dear ImGui (docking) and OpenGL 3.3.

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](LICENSE)

> **Status:** early development. A functional archive browser front-end (`M2`) sits on a solid container engine (`M1`). Versioning follows milestones when the feature name changes — you may see `0.x.0 (Mn)` in the About box.

## Download

GNU/Linux x86_64 installers are attached to each [GitHub release](https://github.com/caiobvilar/GRFEditor-CPP/releases/latest):

| Package | Install |
| --- | --- |
| [`.deb` (Debian / Ubuntu)](https://github.com/caiobvilar/GRFEditor-CPP/releases/latest) | `sudo apt install ./grfeditor-*.deb` |
| [`.rpm` (Fedora / openSUSE / RHEL)](https://github.com/caiobvilar/GRFEditor-CPP/releases/latest) | `sudo dnf install ./grfeditor-*.rpm` |

Each release also ships a `SHA256SUMS` file. A text-mode CLI (`grfcl`) ships in the same packages. Packages can be produced locally with CMake/CPack — see "Building" below.

## Features

- **Containers**
  - GRF v2.0 and v3.0 (int64 offsets): open, create, add, replace, remove, extract, save/repack
  - GPF (single v2 header, wedged at version 0x200) via the same reader
  - Thor patch (modes `0x30` multi-file and `0x21` single-file, zlib-compressed, `ASSF` footer) — read-only
  - RGZ (Calendar gzip-wrapped composite) — read-only
  - Standard `/` and `\` separators accepted; case-insensitive lookup; CP1252 → UTF-8 name handling
  - DES filename decryption for header entries
  - LZSS and zlib decompression; zlib-compressed, alignment-padded storage on write
  - "Repack/defragment" baseline-compatible rewriter
- **GUI** (`grfeditor`)
  - Dockable Dear ImGui shell: file explorer, preview, properties, and a status log pane
  - Live filter (substring, case-insensitive), text + hex preview of small entries
  - Add / extract / remove / save via native dialogs (zenity); read-only archives are guarded
  - Dark theme with classic-RO sort of coolant palette
- **CLI** (`grfcl`) — script-friendly container operations, see below
- **Tests** — GoogleTest suite covering the whole container engine (run with `ctest`)

### Not yet (roadmap)

- `M3` — sprite (`.spr`) and palette (`.pal`) preview, render selected entries from the tree
- `M4` — ACT, STR and map tooling (the GUI menu stubs already exist)
- `M5` — GRF shrinker / validator / *GrfEditorCrypt* support, drag-and-drop, cross-platform packaging (Windows/macOS)

## Command-line usage

```
  grfcl info <file>                     show header + table info
  grfcl ls <file> [pattern]             list entries (substring, case-insensitive)
  grfcl extract <file> <path> [-o dir]  decompress one entry to stdout or -o file
  grfcl extract-all <file> -o dir       decompress every entry into dir
  grfcl add <file> <path> <src>         add src into the archive (saves in place)
  grfcl remove <file> <path>            delete an entry (saves in place)
  grfcl save <file> [out]               repack (defragment); default writes in place
  grfcl verify <file>                   decompress every entry, report failures
```

Extensions drive dispatch: `.grf`/`.gpf` → GRF, `.thor`/`.thm`/`.thz` → Thor, `.rgz` → RGZ.

## Building from source

Prerequisites: CMake ≥ 3.25, a C++20 compiler (GCC ≥ 12 / Clang ≥ 15), Python ≥ 3.8 and [Conan 2](https://conan.io). `zenity` is required only by the GUI's native file dialogs.

```sh
python3 -m pip install conan
conan profile detect --force

conan install . -of build -s build_type=Release --build=missing
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$PWD/build/conan_toolchain.cmake"
cmake --build build -j "$(nproc)"
```

Run it:

```sh
./build/grfeditor          # GUI
./build/grfcl info data.grf
```

## Testing

```sh
ctest --test-dir build --output-on-failure
```

The test binary (`grfcore_tests`, GoogleTest) drives the whole engine without any GUI or display: zlib/LZSS round-trips, DES, header marshaling, CP1252, v2/v3 containers, THOR modes `0x30`/`0x21`, RGZ, and provider dispatch.

## Project layout

```
CMakeLists.txt        build, install and CPack rules (.deb / .rpm)
conanfile.py          dependency manifest (SDL3, imgui, glad, zlib, gtest)
src/
  grfcore/            M1 container engine: GRF/GPF/Thor/RGZ, DES, compression
  app/                SDL3 + ImGui shell: dockspace, archive panel
  grfmedia/           future M3 sprite/ACT parsing (early)
  grfcl/              command-line front-end
  main.cpp            GUI entry point
tests/                GoogleTest suite
packaging/            .desktop launcher (installed by CMake)
resources/            icons
```

## Acknowledgements

Original concept and file-format know-how from **Tokeiburu**'s [GRF Editor](https://github.com/Tokeiburu/GRFEditor) — a Windows-only WPF editor whose CLI, reader architecture and format handling this project reimplements in C++ for Linux first. This work is unaffiliated with Gravity. Ragnarok Online and related marks belong to their respective right holders and this project is not endorsed by them.

## License

Distributed under the Creative Commons **Attribution-NonCommercial-ShareAlike 4.0 International** license, matching the license under which the original GRF Editor is distributed. See [LICENSE](LICENSE) for the full text. Contributions are accepted under the same terms.