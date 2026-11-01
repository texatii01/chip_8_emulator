# CHIP-8 Emulator

A CHIP-8 interpreter with an SDL2 window, keyboard input, and sound-timer
beep. The emulator targets the original 64 × 32 CHIP-8 display.

The current stable release is **1.0.1**.

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
./build/chip_8_emulator path/to/rom.ch8
```

Replace `path/to/rom.ch8` with the path to a CHIP-8 ROM file you provide.
The emulator does not include a ROM.

On Windows, open `chip_8_emulator.slnx` in Visual Studio and build the
`chip_8_emulator` project. Visual Studio restores SDL2 from `vcpkg.json`.
Run the resulting executable with a ROM path as its first argument.

To build with CMake on Windows, configure with the vcpkg toolchain:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
.\build\Release\chip_8_emulator.exe path\to\rom.ch8
```

On macOS, select **Run CHIP-8 emulator** in VS Code's **Run and Debug** view.
The configuration prompts for a ROM path; its build task uses CMake and LLDB.
On Windows, use the Visual Studio solution or configure VS Code's CMake task
with the vcpkg toolchain.

## ROMs

The emulator requires a ROM, but no ROM is included in this repository. By
default, it looks for `pong.ch8` beside the executable. If you provide that
file in the repository root before building, the build copies it beside the
executable. Otherwise, pass a ROM path as the first argument:

```sh
./build/chip_8_emulator path/to/rom.ch8
```

The ROM must be no larger than the available CHIP-8 program memory. ROM files
are not required to build the emulator.

See [USER_GUIDE.md](USER_GUIDE.md) for controls and implementation notes.

## License

This project is distributed under the [MIT License](LICENSE).
