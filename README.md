# Kryne Engine 2

Kryne Engine 2 is a from-scratch, cross-platform game engine (and personal R&D project) written in modern C++23,
covering everything from the low-level RHI up to render-graph-driven rendering pipelines and sample applications.

## Philosophy

Kryne Engine is designed to be used as a set of building blocks rather than a monolithic framework. The engine
[`Core/`](Core) is kept as minimal as possible, providing only the base components (RHI, threading, memory,
platform, math) that every other part of the engine relies on. Everything beyond that is built as a
[`Modules/`](Modules) that depends on the core — and, where relevant, on other modules — so that users only need
to pull in the parts of the engine they actually need.

The project ships premade modules so you don't have to reimplement common functionality (file system, GUI,
render graph, etc.) from scratch, but they are meant as a convenient starting point, not a mandatory one. You're
encouraged to rewrite any module to fit your needs, or to write entirely new ones on top of the core.

## Features

In line with the philosophy above, features are split between the minimal [`Core/`](Core) and the opt-in
[`Modules/`](Modules) built on top of it.

### Core

- **Multi-backend graphics RHI** — a unified graphics API abstraction over:
  - Vulkan (Windows, Linux, macOS)
  - DirectX 12 (Windows)
  - Metal 4 (macOS)
- Multithreaded job/task system
- Platform, memory, and math primitives shared by every other engine component

### Modules

- **Render graph** for declarative, dependency-driven frame construction (plus a companion debug module)
- File system, GUI, and ImGui integration
- Shader reflection library
- SDF text and texture rendering
- Physics integration (via [Box3D](Modules/Box3D))
- ...and more under [`Modules/`](Modules), each independent and only pulled in when needed

### Samples & tests

- Sample applications ([`Samples/`](Samples)) demonstrating engine features
- Unit tests for core systems and modules ([`Tests/`](Tests))

## Repository layout

| Directory  | Description                                                        |
|------------|---------------------------------------------------------------------|
| `Core/`    | Engine core: graphics RHI, threading, platform, memory, math        |
| `Modules/` | Optional engine modules (render graph, ImGui, file system, etc.)    |
| `Samples/` | Sample applications built on top of the engine                      |
| `Tools/`   | Companion tools (shader compiler, project manager), git submodule                  |
| `Tests/`   | Unit tests for `Core/` and `Modules/`                                |
| `Docs/`    | Design/architecture documentation                                   |
| `External/`| Third-party dependencies (git submodule)                            |
| `CMake/`   | CMake build system helpers (build config, shader compilation, etc.) |

## Getting started

### Prerequisites

- CMake 3.20+
- A C++23-capable compiler (MSVC, Clang, or GCC)
- [Ninja](https://ninja-build.org/) (recommended generator)
- Python 3 (used by the shader build pipeline)
- The graphics SDK matching your target backend:
  - **Vulkan SDK** on Windows/Linux
  - **Xcode / Metal** toolchain on macOS
  - Windows SDK (DirectX 12) on Windows

### Clone

The project uses git submodules for its dependencies and tools, so clone recursively:

```bash
git clone --recursive https://github.com/Mcgode/KryneEngine.git
```

(or run `git submodule update --init --recursive` after a regular clone)

### Build

```bash
cmake -B build -G Ninja -DGraphicsApi=VK   # or MTL / DX12
cmake --build build
```

`GraphicsApi` defaults to `VK` (Vulkan) if not specified. Additional `KRYNE_ENGINE_*` CMake options let you toggle
building modules, tools, samples, tests, and profiling/instrumentation — see [`CMakeLists.txt`](CMakeLists.txt).

### Run the tests

```bash
ctest --test-dir build
```

## Documentation

API and architecture documentation is generated via Doxygen from the [`Doxyfile`](Doxyfile), with additional
design notes under [`Docs/`](Docs).

## Use of AI

Parts of this project — including code, tests, and documentation — are developed with the assistance of AI coding
tools (such as Claude Code), used as a pair-programming aid for refactoring, code audits, bug fixing, and
write-ups. All AI-assisted changes are reviewed, tested, and iterated on by the maintainer before being merged;
the engine's architecture, design decisions, and new feature implementation remain human-driven. Commits and pull
requests that include significant AI-generated content are attributed accordingly (e.g. `Co-Authored-By`
trailers).

Non-exhaustive list of areas where AI assistance has been used:
- Brainstorming, state-of-the-art investigation, and design discussions
- Documentation
- Refactoring and auditing engine/module code
- Generating sample code
- Bug investigating and fixing
- Writing and extending unit tests
- Build tooling (CMake) and CI workflows
