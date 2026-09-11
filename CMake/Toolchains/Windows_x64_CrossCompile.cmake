# Toolchain for cross-compiling from macOS -> Windows (x86_64).
#
# Rationale: the target is a mingw-w64 sysroot (headers + CRT + import libs coming
# from `brew install mingw-w64`), but the *compiler* is LLVM/Clang rather than
# mingw's GCC. Several third-party dependencies (EASTL / EAThread in particular)
# use MSVC-isms such as SEH `__try`/`__except` that GCC rejects outright; Clang
# accepts them for Windows targets. Clang also links through LLD, so no separate
# cross binutils build is required.
#
# Requirements:
#   brew install mingw-w64 llvm lld
#
# Usage:
#   cmake -S . -B <build> -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=CMake/Toolchains/Windows_x64_CrossCompile.cmake \
#     -DCMAKE_BUILD_TYPE="Debug DX12"

set(CMAKE_SYSTEM_NAME Windows CACHE STRING "Target system name" FORCE)
set(CMAKE_SYSTEM_PROCESSOR x86_64 CACHE STRING "Target processor" FORCE)

set(KE_TARGET_TRIPLE x86_64-w64-mingw32)

# Clear macOS-specific settings that CLion / the bundled CMake may inject; these
# add `-arch arm64` / `-isysroot <macOS SDK>` to compiler invocations and break
# the cross build.
set(CMAKE_OSX_ARCHITECTURES "" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "" FORCE)
set(CMAKE_OSX_SYSROOT "" CACHE STRING "" FORCE)

# --- Locate the mingw-w64 sysroot -------------------------------------------------
# The sysroot is the directory that *contains* the `x86_64-w64-mingw32/` tree, i.e.
# the parent of the directory holding `x86_64-w64-mingw32-gcc`.
find_program(KE_MINGW_GCC ${KE_TARGET_TRIPLE}-gcc)
if (NOT KE_MINGW_GCC)
    message(FATAL_ERROR
        "Could not find ${KE_TARGET_TRIPLE}-gcc. Install the mingw-w64 sysroot: brew install mingw-w64")
endif ()
# Homebrew installs `x86_64-w64-mingw32-gcc` as a chain of symlinks into the
# versioned Cellar keg; resolve it so the sysroot is the real toolchain dir and
# not `/opt/homebrew`.
get_filename_component(KE_MINGW_GCC "${KE_MINGW_GCC}" REALPATH)
get_filename_component(_ke_mingw_bin "${KE_MINGW_GCC}" DIRECTORY)
get_filename_component(KE_MINGW_ROOT "${_ke_mingw_bin}" DIRECTORY)

# --- Locate LLVM/Clang ----------------------------------------------------------
# Must be a full LLVM (Homebrew keg or similar), not Apple Clang: Apple Clang has
# neither the mingw driver nor LLD.
find_program(KE_CLANG   clang   HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin /opt/homebrew/bin)
find_program(KE_CLANGXX clang++ HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin /opt/homebrew/bin)
if (NOT KE_CLANG OR NOT KE_CLANGXX)
    message(FATAL_ERROR "Could not find LLVM clang/clang++. Install it: brew install llvm lld")
endif ()

find_program(KE_LLVM_AR      llvm-ar      HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
find_program(KE_LLVM_RANLIB  llvm-ranlib  HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
find_program(KE_LLVM_RC      llvm-rc      HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)
find_program(KE_MINGW_WINDRES ${KE_TARGET_TRIPLE}-windres HINTS "${_ke_mingw_bin}")

set(CMAKE_C_COMPILER   "${KE_CLANG}"   CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${KE_CLANGXX}" CACHE FILEPATH "" FORCE)
set(CMAKE_C_COMPILER_TARGET   ${KE_TARGET_TRIPLE})
set(CMAKE_CXX_COMPILER_TARGET ${KE_TARGET_TRIPLE})
set(CMAKE_ASM_COMPILER   "${KE_CLANG}" CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER_TARGET ${KE_TARGET_TRIPLE})

if (KE_LLVM_AR)
    set(CMAKE_AR "${KE_LLVM_AR}" CACHE FILEPATH "" FORCE)
endif ()
if (KE_LLVM_RANLIB)
    set(CMAKE_RANLIB "${KE_LLVM_RANLIB}" CACHE FILEPATH "" FORCE)
endif ()
if (KE_MINGW_WINDRES)
    set(CMAKE_RC_COMPILER "${KE_MINGW_WINDRES}" CACHE FILEPATH "" FORCE)
elseif (KE_LLVM_RC)
    set(CMAKE_RC_COMPILER "${KE_LLVM_RC}" CACHE FILEPATH "" FORCE)
endif ()

# Point Clang at the mingw sysroot and force LLD as the linker.
#   --sysroot / CMAKE_SYSROOT : headers + CRT + Win32 import libs
#   -B <mingw bin>            : lets Clang's MinGW driver discover the GCC install
#                               (libgcc / libgcc_eh search paths, mingw environment)
#   -fms-extensions           : enables __try / __except used by EAThread
#   -include sal.h            : EASTL's Win32 shims (EAThread futex, EAStopwatch)
#                               use SAL annotations (_Out_, _Inout_, ...) without
#                               pulling in a header that defines them; mingw ships
#                               an empty <sal.h> that satisfies them.
set(CMAKE_SYSROOT "${KE_MINGW_ROOT}")
set(_ke_common_flags "-B${_ke_mingw_bin} -fms-extensions -include sal.h")
set(CMAKE_C_FLAGS_INIT   "${_ke_common_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_ke_common_flags}")
set(CMAKE_ASM_FLAGS_INIT "-B${_ke_mingw_bin}")
foreach (_ke_lt EXE SHARED MODULE)
    set(CMAKE_${_ke_lt}_LINKER_FLAGS_INIT "-fuse-ld=lld -B${_ke_mingw_bin}")
endforeach ()

# Statically link the mingw runtime (libstdc++, libgcc, libwinpthread, ...) into
# every executable. Those DLLs don't exist on a stock Windows machine and aren't
# picked up by CMake's TARGET_RUNTIME_DLLS, so without this the binaries only run
# on a box that has the mingw toolchain. The genuine Win32 system DLLs (kernel32,
# d3d12, dxgi, ...) stay dynamically imported - they only ship as import libs.
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -static")

# --- find_* behaviour ---------------------------------------------------------
# /opt/homebrew/mingw-deps holds hand-built target deps (freetype, libpng, zlib).
# Historically this lived under /tmp/mingw; keep that as a fallback.
set(CMAKE_FIND_ROOT_PATH
    /opt/homebrew/mingw-deps
    /tmp/mingw
    "${KE_MINGW_ROOT}"
    "${KE_MINGW_ROOT}/${KE_TARGET_TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()
