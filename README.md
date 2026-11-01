# CHIP-8 Emulator

A CHIP-8 interpreter with an SDL2 window, keyboard input, and sound-timer
beep. The emulator targets the original 64 × 32 CHIP-8 display.

The current stable release is **1.0.0**.

## Requirements

- A C++17 compiler
- CMake 3.16 or later
- SDL2 development libraries

On macOS, install the build tools and SDL2 with Homebrew:

```sh
brew install cmake sdl2
```

On Windows, install Visual Studio with the **Desktop development with C++**
workload and install vcpkg. The repository includes a vcpkg manifest for SDL2.

## Build and run

From the repository root on macOS or Linux:

```sh
cmake -S . -B build
cmake --build build
./build/chip_8_emulator
```

On Windows, open `chip_8_emulator.slnx` in Visual Studio and build the
`chip_8_emulator` project. Visual Studio restores SDL2 from `vcpkg.json`.

To build with CMake on Windows, configure with the vcpkg toolchain:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
.\build\Release\chip_8_emulator.exe
```

On macOS, the included VS Code configuration is available through **Run and
Debug**: select **Run CHIP-8 emulator** and start debugging. Its build task
uses CMake and LLDB. On Windows, use the Visual Studio solution or configure
VS Code's CMake task with the vcpkg toolchain.

## ROMs

When `pong.ch8` is present in the repository root, the build copies it beside
the executable and the emulator loads it by default. To run another ROM, pass
its path as the first argument:

```sh
./build/chip_8_emulator path/to/rom.ch8
```

The ROM must be no larger than the available CHIP-8 program memory. ROM files
are not required to build the emulator.

See [USER_GUIDE.md](USER_GUIDE.md) for controls and implementation notes.
