# Pi-Engine

Open source 3D engine, optimized for **Raspberry Pi 4** (forward-compatible with Pi5),
targeting low-poly retro-style games as the primary use case. C++20, Vulkan 1.2 core.

Full context and rationale: see [`CLAUDE.md`](CLAUDE.md) and the design documents in
[`docs/`](docs/). Current status: **M0 — Hello Vulkan** through **M2 — Hello Scene** are
all verified end to end on Linux desktop and **physical Pi4 hardware** — M2 populates a
64-entity ECS scene and culls it in parallel via the Job System, rendering only what's
inside the camera frustum through the real V3D (VideoCore) Vulkan driver (see
[`samples/`](samples/)).

## Prerequisites

- CMake ≥ 3.24, Ninja (Linux/macOS) or Visual Studio 2022 (Windows).
- [vcpkg](https://github.com/microsoft/vcpkg), with `VCPKG_ROOT` set in your environment —
  dependencies are pinned in [`vcpkg.json`](vcpkg.json) (manifest mode, no manual install
  needed beyond that) *except* the autotools below, which vcpkg itself cannot provide.
- **Linux only**: `autoconf`, `automake`, `libtool`, `autoconf-archive` — SDL2 pulls in
  `libxcrypt` on Linux, and vcpkg builds it from source via autotools instead of CMake.
  ```
  sudo apt install autoconf autoconf-archive automake libtool   # Debian/Ubuntu
  sudo dnf install autoconf autoconf-archive automake libtool   # Fedora
  ```
- [Vulkan SDK](https://vulkan.lunarg.com/) — provides `glslc`, used to compile
  `shaders/*.vert`/`*.frag` to SPIR-V at build time (see `shaders/README.md`), and is the
  usual source of the Vulkan validation layers used in debug builds. Needs to be on `PATH`
  or discoverable via the `VULKAN_SDK` environment variable (the SDK's own `setup-env.sh`
  does both).
- **Cross-compiling for Pi4/Pi5** from Linux x86_64 (the primary dev workflow, see
  [`docs/02-mvp-roadmap-analysis.md`](docs/02-mvp-roadmap-analysis.md) section 3): an
  `aarch64-linux-gnu` cross toolchain, e.g. on Debian/Ubuntu:
  ```
  sudo apt install g++-aarch64-linux-gnu gcc-aarch64-linux-gnu
  ```

## Build

Presets (`CMakePresets.json`): `pi4`, `pi5`, `linux-pc`, `windows`.

```
cmake --preset linux-pc
cmake --build --preset linux-pc-debug
ctest --preset linux-pc-debug
```

Cross-compiling for Pi4:

```
cmake --preset pi4
cmake --build --preset pi4-debug
```

Building *directly on* a Pi4/Pi5 (native ARM, no cross toolchain) instead of cross-compiling:
the `pi4`/`pi5` presets chainload `cmake/toolchains/aarch64-linux-gnu.cmake`, which hardcodes
the cross-compiler names (`aarch64-linux-gnu-gcc`) — those binaries don't exist under that
name on a native ARM system (there it's just `gcc`). Override the chainload to build natively:
```
cmake --preset pi4 -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=
cmake --build build/pi4 --config Debug
```

## Repository layout

See `CLAUDE.md` section 6. Every `engine/include/engine/<module>/` directory has its own
`README.md` listing which milestone (M0-M5) populates it and with which files — the exact
plan is in [`docs/03-technical-analysis-claude-code.md`](docs/03-technical-analysis-claude-code.md).

## Roadmap

| # | Milestone | Status |
|---|---|---|
| M0 | Hello Vulkan | Verified on Linux desktop and physical Pi4 hardware |
| M1 | Hello Mesh | Verified on Linux desktop and physical Pi4 hardware |
| M2 | Hello Scene | Verified on Linux desktop and physical Pi4 hardware |
| M3 | Hello Script | Not started |
| M4 | Hello Physics | Not started |
| M5 | Vertical Slice (target) | Not started |

Details: `CLAUDE.md` section 8, [`docs/02-mvp-roadmap-analysis.md`](docs/02-mvp-roadmap-analysis.md).
