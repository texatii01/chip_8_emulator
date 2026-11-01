# CHIP-8 Emulator

A CHIP-8 interpreter with an SDL2 window, keyboard input, and sound-timer
beep. The emulator targets the original 64 × 32 CHIP-8 display.

The current stable release is **1.1.0**.

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
For a ready-to-run Windows build, download the ZIP from the project's
[GitHub Releases](https://github.com/texatii01/chip_8_emulator/releases) page,
extract it, and open `chip_8_emulator.exe`.

## Build and run

From the repository root on macOS or Linux:

```sh
cmake -S . -B build
cmake --build build
./build/chip_8_emulator
```

The launcher lets you browse to a ROM and choose the pixel color. The
emulator does not include a ROM.

To skip the launcher and start a specific ROM directly:

```sh
./build/chip_8_emulator path/to/rom.ch8
```

On Windows, open `chip_8_emulator.slnx` in Visual Studio and build the
`chip_8_emulator` project. Visual Studio restores SDL2 from `vcpkg.json`.
Run the resulting executable without arguments to open the launcher, or pass
a ROM path as its first argument to start it directly.

To build with CMake on Windows, configure with the vcpkg toolchain:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
.\build\Release\chip_8_emulator.exe
```

Pass `path\to\rom.ch8` as an argument to bypass the launcher.

On macOS, select **Run CHIP-8 emulator** in VS Code's **Run and Debug** view.
Without a ROM argument, the app opens its launcher, where you can browse to a
ROM and choose the pixel color. Its build task uses CMake and LLDB.
On Windows, use the Visual Studio solution or configure VS Code's CMake task
with the vcpkg toolchain.

Each version tag also builds a Windows x64 ZIP and attaches it to the matching
GitHub Release. To package an existing tag, run the **Windows release** workflow
from the Actions tab and enter its tag.

## ROMs

The emulator requires a ROM, but no ROM is included in this repository. Without
a command-line ROM path, the graphical launcher lets you browse folders and
select a ROM (`.ch8`, `.chip8`, `.c8`, or `.rom`). If `pong.ch8` is beside the
executable, it is selected by default. You can also launch a ROM directly by
passing its path as the first argument:

```sh
./build/chip_8_emulator path/to/rom.ch8
```

The ROM must be no larger than the available CHIP-8 program memory. ROM files
are not required to build the emulator.

See [USER_GUIDE.md](USER_GUIDE.md) for controls and implementation notes.

## License

This project is distributed under the [MIT License](LICENSE).
