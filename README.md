# Pi-Engine

Open source 3D engine, optimized for **Raspberry Pi 4** (forward-compatible with Pi5),
targeting low-poly retro-style games as the primary use case. C++20, Vulkan 1.2 core.

Full context and rationale: see [`CLAUDE.md`](CLAUDE.md) and the design documents in
[`docs/`](docs/). Current status: scaffold in place, milestone **M0 — Hello Vulkan** not
yet implemented (see [`samples/m0_hello_vulkan/`](samples/m0_hello_vulkan/)).

## Prerequisites

- CMake ≥ 3.24, Ninja (Linux/macOS) or Visual Studio 2022 (Windows).
- [vcpkg](https://github.com/microsoft/vcpkg), with `VCPKG_ROOT` set in your environment —
  dependencies are pinned in [`vcpkg.json`](vcpkg.json) (manifest mode, no manual install
  needed beyond that).
- **Cross-compiling for Pi4/Pi5** from Linux x86_64 (the primary dev workflow, see
  [`docs/02-analisi-mvp-roadmap.md`](docs/02-analisi-mvp-roadmap.md) section 3): an
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

## Repository layout

See `CLAUDE.md` section 6. Every `engine/include/engine/<module>/` directory has its own
`README.md` listing which milestone (M0-M5) populates it and with which files — the exact
plan is in [`docs/03-analisi-tecnica-claude-code.md`](docs/03-analisi-tecnica-claude-code.md).

## Roadmap

| # | Milestone | Status |
|---|---|---|
| M0 | Hello Vulkan | Scaffold ready, not implemented |
| M1 | Hello Mesh | Not started |
| M2 | Hello Scene | Not started |
| M3 | Hello Script | Not started |
| M4 | Hello Physics | Not started |
| M5 | Vertical Slice (target) | Not started |

Details: `CLAUDE.md` section 8, [`docs/02-analisi-mvp-roadmap.md`](docs/02-analisi-mvp-roadmap.md).
