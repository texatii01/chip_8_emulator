# User Guide

## Controls

The keyboard maps to the CHIP-8 keypad as follows:

| Keyboard | CHIP-8 keys |
|----------|-------------|
| `1 2 3 4` | `1 2 3 C` |
| `Q W E R` | `4 5 6 D` |
| `A S D F` | `7 8 9 E` |
| `Z X C V` | `A 0 B F` |

Press and hold a mapped key to keep its CHIP-8 key pressed. Press **Escape**
or close the window to quit.

## ROM selection

No ROM is included with the emulator. Provide your own ROM to run. When you
start the emulator without a command-line argument, the launcher opens with
the executable's directory. Click a folder to browse into it, select a ROM,
then click **Launch**. The parent-directory button and mouse wheel help you
navigate; the arrow keys move the selection, **Enter** opens a folder or
launches the selected ROM, and **Backspace** moves to the parent folder. Only
`.ch8`, `.chip8`, `.c8`, and `.rom` files are shown.

If `pong.ch8` is beside the executable, it is preselected. To skip the launcher,
pass a ROM path as the first command-line argument:

```sh
./build/chip_8_emulator path/to/game.ch8
```

The argument can be an absolute path or a path relative to the current working
directory.

## Pixel color

Choose one of the ten color swatches in the launcher or set a custom color
using the red, green, and blue sliders. The preview shows the selected color.
The chosen color applies to all lit pixels for that run; the background stays
dark.

## Emulator behavior

- The display is 64 × 32 pixels and scales to fit the resizable window.
- Instructions run at approximately 700 instructions per second.
- Delay and sound timers tick at 60 Hz.
- A 440 Hz tone plays while the sound timer is nonzero. If SDL cannot open an
  audio device, the emulator continues without sound and reports a warning.
- The instruction set covers the original CHIP-8 system. Super-CHIP and
  XO-CHIP extensions are not implemented.

Some CHIP-8 instructions have different historical interpreter behaviors.
This emulator uses the behavior documented in its opcode comments; ROMs that
expect another interpreter profile may behave differently.
