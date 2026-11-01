#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <SDL.h>

#include "launcher_ui.h"

namespace {

constexpr std::size_t kMemorySize = 0x1000;
constexpr std::size_t kProgramStartAddress = 0x200;
constexpr std::size_t kScreenWidth = 64;
constexpr std::size_t kScreenHeight = 32;
constexpr std::size_t kRegisterCount = 16;
constexpr std::size_t kKeyCount = 16;
constexpr std::size_t kStackDepth = 16;
constexpr std::size_t kSpriteWidth = 8;
constexpr std::size_t kFontBytesPerCharacter = 5;
constexpr std::size_t kFontCharacterCount = 16;
constexpr std::size_t kOpcodeSize = 2;
constexpr std::size_t kBcdDigitCount = 3;
constexpr uint8_t kDecimalBase = 10;
constexpr uint8_t kHundredsPlace = 100;
constexpr int kWindowScale = 12;
constexpr int kLauncherWindowWidth = 1000;
constexpr int kLauncherWindowHeight = 700;
constexpr double kInstructionsPerSecond = 700.0;
constexpr double kTimerUpdatesPerSecond = 60.0;
constexpr int kAudioSampleRate = 44100;
constexpr int kAudioToneFrequency = 440;
constexpr std::size_t kAudioFramesPerChunk = kAudioSampleRate / 30;
constexpr std::size_t kMaximumQueuedAudioFrames = kAudioSampleRate / 10;
constexpr int16_t kAudioAmplitude = 6000;

} // namespace

// Each hexadecimal character uses five bytes, one byte per 8-pixel row.
constexpr std::array<uint8_t, kFontBytesPerCharacter * kFontCharacterCount>
    font_sprites = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

// Holds the CHIP-8 processor's registers, timers, program position, and calls.
struct CPU {
  std::array<uint8_t, kRegisterCount> registers = {
      0};                      // V0-VF general-purpose registers
  uint16_t index_register = 0; // I: address used by memory opcodes
  uint16_t program_counter = kProgramStartAddress; // First program address
  uint8_t delay_timer = 0;   // A 60 Hz timer loop should decrement this value
  uint8_t sound_timer = 0;   // Sound should play while this value is nonzero
  uint8_t stack_pointer = 0; // Number of saved return addresses
  std::array<uint16_t, kStackDepth> return_stack = {0}; // Subroutine addresses
};

// The memory bus groups the state opcodes read and modify outside the CPU.
struct MemoryBus {
  std::array<uint8_t, kMemorySize> memory = {0};
  std::array<uint8_t, kScreenWidth * kScreenHeight> screen = {0};
  std::array<uint8_t, kKeyCount> keypad = {0};
};

// Owns the SDL window and translates keyboard/window events for the emulator.
class SDLFrontend {
public:
  ~SDLFrontend() { shutdown(); }

  bool initialize() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
      std::cerr << "Error: Could not initialize SDL: " << SDL_GetError()
                << std::endl;
      return false;
    }
    sdl_initialized_ = true;

    window_ = SDL_CreateWindow("CHIP-8 Emulator", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, kLauncherWindowWidth,
                               kLauncherWindowHeight, SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr) {
      std::cerr << "Error: Could not create the SDL window: " << SDL_GetError()
                << std::endl;
      return false;
    }

    renderer_ = SDL_CreateRenderer(
        window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer_ == nullptr) {
      std::cerr << "SDL accelerated renderer unavailable: " << SDL_GetError()
                << ". Trying software rendering." << std::endl;
      renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    } else {
      renderer_has_vsync_ = true;
    }
    if (renderer_ == nullptr) {
      std::cerr << "Error: Could not create an SDL renderer: " << SDL_GetError()
                << std::endl;
      return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_RenderSetLogicalSize(renderer_, static_cast<int>(kScreenWidth),
                                 static_cast<int>(kScreenHeight)) != 0) {
      std::cerr << "Error: Could not configure the SDL display: "
                << SDL_GetError() << std::endl;
      return false;
    }

    SDL_AudioSpec desired_audio{};
    desired_audio.freq = kAudioSampleRate;
    desired_audio.format = AUDIO_S16SYS;
    desired_audio.channels = 1;
    desired_audio.samples = 1024;
    audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &desired_audio,
                                        &audio_specification_, 0);
    if (audio_device_ == 0) {
      std::cerr << "Warning: Could not open audio device; sound is disabled: "
                << SDL_GetError() << std::endl;
    } else {
      SDL_PauseAudioDevice(audio_device_, 0);
    }

    return true;
  }

  SDL_Renderer *renderer() const { return renderer_; }

  SDL_Window *window() const { return window_; }

  bool prepare_game_display() const {
    if (SDL_RenderSetLogicalSize(renderer_, static_cast<int>(kScreenWidth),
                                 static_cast<int>(kScreenHeight)) != 0) {
      std::cerr << "Error: Could not configure the emulator display: "
                << SDL_GetError() << std::endl;
      return false;
    }

    SDL_SetWindowSize(window_, static_cast<int>(kScreenWidth) * kWindowScale,
                      static_cast<int>(kScreenHeight) * kWindowScale);
    SDL_SetWindowTitle(window_, "CHIP-8 Emulator");
    return true;
  }

  // Returns false when the user closes the window or presses Escape.
  bool poll_events(MemoryBus &memory_bus) const {
    SDL_Event event;
    bool keep_running = true;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        keep_running = false;
        continue;
      }

      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        memory_bus.keypad.fill(0);
        continue;
      }

      if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) {
        continue;
      }

      const bool key_pressed = event.type == SDL_KEYDOWN;
      if (key_pressed && event.key.repeat != 0) {
        continue;
      }
      if (key_pressed && event.key.keysym.sym == SDLK_ESCAPE) {
        keep_running = false;
        continue;
      }

      uint8_t chip8_key;
      if (translate_key(event.key.keysym.sym, chip8_key)) {
        memory_bus.keypad[chip8_key] = key_pressed;
      }
    }

    return keep_running;
  }

  // Draws each lit CHIP-8 pixel into the scaled, logical-size SDL window.
  bool render(const MemoryBus &memory_bus, SDL_Color pixel_color) const {
    if (SDL_SetRenderDrawColor(renderer_, 12, 16, 20, 255) != 0 ||
        SDL_RenderClear(renderer_) != 0) {
      std::cerr << "Error: Could not clear the SDL display: " << SDL_GetError()
                << std::endl;
      return false;
    }

    if (SDL_SetRenderDrawColor(renderer_, pixel_color.r, pixel_color.g,
                               pixel_color.b, pixel_color.a) != 0) {
      std::cerr << "Error: Could not set the SDL pixel color: "
                << SDL_GetError() << std::endl;
      return false;
    }

    for (std::size_t y = 0; y < kScreenHeight; ++y) {
      for (std::size_t x = 0; x < kScreenWidth; ++x) {
        if (!memory_bus.screen[y * kScreenWidth + x]) {
          continue;
        }

        SDL_Rect pixel = {static_cast<int>(x), static_cast<int>(y), 1, 1};
        if (SDL_RenderFillRect(renderer_, &pixel) != 0) {
          std::cerr << "Error: Could not draw a screen pixel: "
                    << SDL_GetError() << std::endl;
          return false;
        }
      }
    }

    SDL_RenderPresent(renderer_);
    return true;
  }

  // Queue a continuous square-wave tone while the CHIP-8 sound timer is active.
  bool update_sound(uint8_t sound_timer) {
    if (audio_device_ == 0) {
      return true;
    }

    if (sound_timer == 0) {
      SDL_ClearQueuedAudio(audio_device_);
      return true;
    }

    const Uint32 maximum_queued_bytes =
        static_cast<Uint32>(kMaximumQueuedAudioFrames * sizeof(int16_t));
    if (SDL_GetQueuedAudioSize(audio_device_) >= maximum_queued_bytes) {
      return true;
    }

    std::array<int16_t, kAudioFramesPerChunk> audio_samples;
    constexpr double phase_increment =
        static_cast<double>(kAudioToneFrequency) / kAudioSampleRate;
    for (int16_t &sample : audio_samples) {
      sample = audio_phase_ < 0.5 ? kAudioAmplitude : -kAudioAmplitude;
      audio_phase_ += phase_increment;
      if (audio_phase_ >= 1.0) {
        audio_phase_ -= 1.0;
      }
    }

    if (SDL_QueueAudio(audio_device_, audio_samples.data(),
                       static_cast<Uint32>(sizeof(audio_samples))) != 0) {
      std::cerr << "Error: Could not queue CHIP-8 audio: " << SDL_GetError()
                << std::endl;
      return false;
    }

    return true;
  }

  // Keep the software-renderer fallback from spinning at an unlimited rate.
  void wait_for_next_frame() const { SDL_Delay(renderer_has_vsync_ ? 1 : 16); }

private:
  static bool translate_key(SDL_Keycode key, uint8_t &chip8_key) {
    // CHIP-8 keypad layout: 123C / 456D / 789E / A0BF.
    switch (key) {
    case SDLK_1:
      chip8_key = 0x1;
      break;
    case SDLK_2:
      chip8_key = 0x2;
      break;
    case SDLK_3:
      chip8_key = 0x3;
      break;
    case SDLK_4:
      chip8_key = 0xC;
      break;
    case SDLK_q:
      chip8_key = 0x4;
      break;
    case SDLK_w:
      chip8_key = 0x5;
      break;
    case SDLK_e:
      chip8_key = 0x6;
      break;
    case SDLK_r:
      chip8_key = 0xD;
      break;
    case SDLK_a:
      chip8_key = 0x7;
      break;
    case SDLK_s:
      chip8_key = 0x8;
      break;
    case SDLK_d:
      chip8_key = 0x9;
      break;
    case SDLK_f:
      chip8_key = 0xE;
      break;
    case SDLK_z:
      chip8_key = 0xA;
      break;
    case SDLK_x:
      chip8_key = 0x0;
      break;
    case SDLK_c:
      chip8_key = 0xB;
      break;
    case SDLK_v:
      chip8_key = 0xF;
      break;
    default:
      return false;
    }

    return true;
  }

  void shutdown() {
    if (audio_device_ != 0) {
      SDL_CloseAudioDevice(audio_device_);
      audio_device_ = 0;
    }
    if (renderer_ != nullptr) {
      SDL_DestroyRenderer(renderer_);
      renderer_ = nullptr;
    }
    if (window_ != nullptr) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
    }
    if (sdl_initialized_) {
      SDL_Quit();
      sdl_initialized_ = false;
    }
  }

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  SDL_AudioDeviceID audio_device_ = 0;
  SDL_AudioSpec audio_specification_{};
  double audio_phase_ = 0.0;
  bool renderer_has_vsync_ = false;
  bool sdl_initialized_ = false;
};

// Prevents opcodes from reading or writing beyond the 4 KiB address space.
bool memory_range_is_valid(const MemoryBus &memory_bus, uint16_t start_address,
                           std::size_t byte_count) {
  return start_address <= memory_bus.memory.size() &&
         byte_count <= memory_bus.memory.size() - start_address;
}

// Loads a ROM into the memory region reserved for CHIP-8 programs.
bool load_rom(MemoryBus &memory_bus, const std::string &file_name) {
  std::ifstream rom_stream(file_name, std::ios::binary | std::ios::ate);
  if (!rom_stream.is_open()) {
    std::cerr << "Error: Could not open " << file_name << "." << std::endl;
    return false;
  }

  std::streamsize rom_byte_count = rom_stream.tellg();
  const std::streamsize maximum_rom_byte_count = static_cast<std::streamsize>(
      memory_bus.memory.size() - kProgramStartAddress);
  if (rom_byte_count < 0 || rom_byte_count > maximum_rom_byte_count) {
    std::cerr << "Error: ROM size does not fit in CHIP-8 memory." << std::endl;
    return false;
  }

  rom_stream.seekg(0, std::ios::beg);
  if (!rom_stream.read(
          reinterpret_cast<char *>(&memory_bus.memory[kProgramStartAddress]),
          rom_byte_count)) {
    std::cerr << "Error: Could not read the complete ROM." << std::endl;
    return false;
  }

  return true;
}

// Fetches one two-byte opcode and advances the program counter past it.
bool fetch_next_opcode(CPU &cpu, const MemoryBus &memory_bus,
                       uint16_t &opcode) {
  if (!memory_range_is_valid(memory_bus, cpu.program_counter, kOpcodeSize)) {
    std::cerr << "Error: Program counter is outside CHIP-8 memory."
              << std::endl;
    return false;
  }

  opcode =
      (static_cast<uint16_t>(memory_bus.memory[cpu.program_counter]) << 8) |
      memory_bus.memory[cpu.program_counter + 1];
  cpu.program_counter += kOpcodeSize;
  return true;
}

// Decodes and executes one instruction. Returns false after reporting an error.
bool execute_opcode(CPU &cpu, MemoryBus &memory_bus, uint16_t opcode) {
  // CHIP-8 notation: X/Y select V-registers; KK is the low byte; NNN
  // is a 12-bit address; N is the final nibble, often a small count.
  uint8_t destination_register_index = (opcode >> 8) & 0x0F;
  uint8_t source_register_index = (opcode >> 4) & 0x0F;
  uint8_t opcode_low_byte = opcode & 0x00FF;
  uint16_t instruction_address = opcode & 0x0FFF;
  uint8_t lowest_nibble = opcode & 0x000F;

  switch (opcode & 0xF000) {
  case 0x0000:
    // 00E0 clears every display pixel. 00EE returns from a subroutine
    // by restoring the address saved by 2NNN. Other 0NNN system-call
    // instructions belong to the original host machine and are ignored.
    if (opcode == 0x00E0) {
      memory_bus.screen.fill(0);
    } else if (opcode == 0x00EE) {
      if (cpu.stack_pointer == 0) {
        std::cerr << "Error: Return instruction with an empty stack."
                  << std::endl;
        return false;
      }
      cpu.program_counter = cpu.return_stack[--cpu.stack_pointer];
    }
    break;

  case 0x1000:
    // 1NNN jumps directly to address NNN without saving a return address.
    cpu.program_counter = instruction_address;
    break;

  case 0x2000:
    // 2NNN calls a subroutine: save the already-advanced PC, then jump.
    if (cpu.stack_pointer < cpu.return_stack.size()) {
      cpu.return_stack[cpu.stack_pointer++] = cpu.program_counter;
      cpu.program_counter = instruction_address;
    } else {
      std::cerr << "Error: CHIP-8 call stack overflow." << std::endl;
      return false;
    }
    break;

  case 0x3000:
    // 3XKK skips the next instruction when VX is equal to byte KK.
    if (cpu.registers[destination_register_index] == opcode_low_byte) {
      cpu.program_counter += kOpcodeSize;
    }
    break;

  case 0x4000:
    // 4XKK skips the next instruction when VX is not equal to KK.
    if (cpu.registers[destination_register_index] != opcode_low_byte) {
      cpu.program_counter += kOpcodeSize;
    }
    break;

  case 0x5000:
    // 5XY0 skips when VX equals VY. The low nibble must be zero.
    if (lowest_nibble == 0 && cpu.registers[destination_register_index] ==
                                  cpu.registers[source_register_index]) {
      cpu.program_counter += kOpcodeSize;
    } else if (lowest_nibble != 0) {
      std::cerr << "Error: Invalid 5XYN opcode." << std::endl;
      return false;
    }
    break;

  case 0x6000:
    // 6XKK loads the immediate byte KK into register VX.
    cpu.registers[destination_register_index] = opcode_low_byte;
    break;

  case 0x7000:
    // 7XKK adds KK to VX. uint8_t assignment intentionally wraps at 255;
    // this opcode does not modify VF on overflow.
    cpu.registers[destination_register_index] += opcode_low_byte;
    break;

  case 0x8000: {
    // Cache operand values before writing results, since either register
    // could be VF and these instructions also update VF as a flag.
    uint8_t destination_value = cpu.registers[destination_register_index];
    uint8_t source_value = cpu.registers[source_register_index];

    switch (lowest_nibble) {
    case 0x0:
      // 8XY0 copies VY into VX.
      cpu.registers[destination_register_index] = source_value;
      break;

    case 0x1:
      // 8XY1 stores the bitwise OR of VX and VY in VX.
      cpu.registers[destination_register_index] =
          destination_value | source_value;
      break;

    case 0x2:
      // 8XY2 stores the bitwise AND of VX and VY in VX.
      cpu.registers[destination_register_index] =
          destination_value & source_value;
      break;

    case 0x3:
      // 8XY3 stores the bitwise XOR of VX and VY in VX.
      cpu.registers[destination_register_index] =
          destination_value ^ source_value;
      break;

    case 0x4: {
      // 8XY4 adds VY to VX. VX receives the low byte, and VF is
      // set to 1 only when the unsigned sum carries past 0xFF.
      uint16_t sum = static_cast<uint16_t>(destination_value) + source_value;
      cpu.registers[destination_register_index] = static_cast<uint8_t>(sum);
      cpu.registers[0xF] = sum > 0xFF;
      break;
    }

    case 0x5:
      // 8XY5 subtracts VY from VX. VF is 1 when no borrow occurs.
      cpu.registers[destination_register_index] =
          destination_value - source_value;
      cpu.registers[0xF] = destination_value >= source_value;
      break;

    case 0x6:
      // 8XY6 shifts VX right by one. VF receives the bit shifted out
      // of the least-significant position (the classic CHIP-8 behavior).
      cpu.registers[destination_register_index] = destination_value >> 1;
      cpu.registers[0xF] = destination_value & 0x1;
      break;

    case 0x7:
      // 8XY7 subtracts VX from VY and stores the result in VX.
      // VF is 1 when this subtraction does not borrow.
      cpu.registers[destination_register_index] =
          source_value - destination_value;
      cpu.registers[0xF] = source_value >= destination_value;
      break;

    case 0xE:
      // 8XYE shifts VX left by one. VF receives the old top bit.
      cpu.registers[destination_register_index] = destination_value << 1;
      cpu.registers[0xF] = (destination_value >> 7) & 0x1;
      break;

    default:
      std::cerr << "Error: Invalid 8XYN opcode." << std::endl;
      return false;
    }
    break;
  }
  case 0x9000:
    // 9XY0 skips the next instruction when VX differs from VY.
    if (lowest_nibble == 0 && cpu.registers[destination_register_index] !=
                                  cpu.registers[source_register_index]) {
      cpu.program_counter += kOpcodeSize;
    } else if (lowest_nibble != 0) {
      std::cerr << "Error: Invalid 9XYN opcode." << std::endl;
      return false;
    }
    break;

  case 0xA000:
    // ANNN loads address NNN into the I index register.
    cpu.index_register = instruction_address;
    break;

  case 0xB000:
    // BNNN jumps to NNN plus V0, providing a register-offset jump.
    cpu.program_counter = instruction_address + cpu.registers[0];
    break;

  case 0xC000:
    // CXKK generates a random byte, ANDs it with KK, and stores the
    // masked result in VX.
    cpu.registers[destination_register_index] =
        (std::rand() & 0xFF) & opcode_low_byte;
    break;

  case 0xD000: {
    // DXYN draws N bytes of an 8-pixel-wide sprite at (VX, VY).
    // Each sprite bit XORs the corresponding screen pixel; VF is set
    // if any lit pixel is erased, which lets games detect collisions.
    // Coordinates wrap at the 64x32 screen edges. A zero height draws
    // no rows in this base CHIP-8 implementation.
    uint8_t screen_start_x =
        cpu.registers[destination_register_index] % kScreenWidth;
    uint8_t screen_start_y =
        cpu.registers[source_register_index] % kScreenHeight;
    cpu.registers[0xF] = 0;

    if (!memory_range_is_valid(memory_bus, cpu.index_register, lowest_nibble)) {
      std::cerr << "Error: Sprite extends beyond CHIP-8 memory." << std::endl;
      return false;
    }

    for (uint8_t sprite_row = 0; sprite_row < lowest_nibble; ++sprite_row) {
      uint8_t sprite_row_bits =
          memory_bus.memory[cpu.index_register + sprite_row];

      for (uint8_t bit_index = 0; bit_index < kSpriteWidth; ++bit_index) {
        if ((sprite_row_bits & (0x80 >> bit_index)) == 0) {
          continue;
        }

        std::size_t screen_pixel_index =
            ((screen_start_y + sprite_row) % kScreenHeight) * kScreenWidth +
            ((screen_start_x + bit_index) % kScreenWidth);

        if (memory_bus.screen[screen_pixel_index]) {
          cpu.registers[0xF] = 1;
        }
        memory_bus.screen[screen_pixel_index] ^= 1;
      }
    }
    break;
  }
  case 0xE000:
    // EX9E skips when the key whose number is in VX is pressed.
    // EXA1 skips when that key is not pressed. VX is masked to the
    // CHIP-8 keypad's valid 0-F range before indexing the key array.
    if (opcode_low_byte == 0x9E &&
        memory_bus.keypad[cpu.registers[destination_register_index] & 0x0F]) {
      cpu.program_counter += kOpcodeSize;
    } else if (opcode_low_byte == 0xA1 &&
               !memory_bus
                    .keypad[cpu.registers[destination_register_index] & 0x0F]) {
      cpu.program_counter += kOpcodeSize;
    } else if (opcode_low_byte != 0x9E && opcode_low_byte != 0xA1) {
      std::cerr << "Error: Invalid EXKK opcode." << std::endl;
      return false;
    }
    break;

  case 0xF000:
    switch (opcode_low_byte) {
    case 0x07:
      // FX07 copies the current delay-timer value into VX.
      cpu.registers[destination_register_index] = cpu.delay_timer;
      break;

    case 0x0A: {
      // FX0A waits for a key press, stores that key number in VX,
      // and keeps the PC on this instruction until a key is available.
      // A frontend must update memory_bus.keypad for this wait to complete.
      bool key_pressed = false;

      for (uint8_t key_index = 0; key_index < memory_bus.keypad.size();
           ++key_index) {
        if (memory_bus.keypad[key_index]) {
          cpu.registers[destination_register_index] = key_index;
          key_pressed = true;
          break;
        }
      }

      if (!key_pressed) {
        cpu.program_counter -= kOpcodeSize;
      }
      break;
    }

    case 0x15:
      // FX15 copies VX into the delay timer.
      cpu.delay_timer = cpu.registers[destination_register_index];
      break;

    case 0x18:
      // FX18 copies VX into the sound timer. The SDL frontend plays a
      // tone for as long as that timer remains nonzero.
      cpu.sound_timer = cpu.registers[destination_register_index];
      break;

    case 0x1E:
      // FX1E adds VX to I, allowing indexed data access relative
      // to the address currently held in I.
      cpu.index_register += cpu.registers[destination_register_index];
      break;

    case 0x29:
      // FX29 points I at the five-byte font glyph for the
      // hexadecimal digit stored in VX.
      cpu.index_register = (cpu.registers[destination_register_index] & 0x0F) *
                           kFontBytesPerCharacter;
      break;

    case 0x33:
      // FX33 stores VX as three decimal digits (hundreds, tens,
      // ones) in consecutive memory bytes beginning at I.
      if (!memory_range_is_valid(memory_bus, cpu.index_register,
                                 kBcdDigitCount)) {
        std::cerr << "Error: BCD write extends beyond CHIP-8 memory."
                  << std::endl;
        return false;
      }
      memory_bus.memory[cpu.index_register] =
          cpu.registers[destination_register_index] / kHundredsPlace;
      memory_bus.memory[cpu.index_register + 1] =
          (cpu.registers[destination_register_index] / kDecimalBase) %
          kDecimalBase;
      memory_bus.memory[cpu.index_register + 2] =
          cpu.registers[destination_register_index] % kDecimalBase;
      break;

    case 0x55:
      // FX55 stores V0 through VX in consecutive bytes beginning
      // at I. This implementation leaves I unchanged.
      if (!memory_range_is_valid(memory_bus, cpu.index_register,
                                 destination_register_index + 1)) {
        std::cerr << "Error: Register save extends beyond CHIP-8 memory."
                  << std::endl;
        return false;
      }
      for (uint8_t register_index = 0;
           register_index <= destination_register_index; ++register_index) {
        memory_bus.memory[cpu.index_register + register_index] =
            cpu.registers[register_index];
      }
      break;

    case 0x65:
      // FX65 loads V0 through VX from consecutive bytes beginning
      // at I. This implementation leaves I unchanged.
      if (!memory_range_is_valid(memory_bus, cpu.index_register,
                                 destination_register_index + 1)) {
        std::cerr << "Error: Register load extends beyond CHIP-8 memory."
                  << std::endl;
        return false;
      }
      for (uint8_t register_index = 0;
           register_index <= destination_register_index; ++register_index) {
        cpu.registers[register_index] =
            memory_bus.memory[cpu.index_register + register_index];
      }
      break;

    default:
      std::cerr << "Error: Invalid FXKK opcode." << std::endl;
      return false;
    }
    break;

  default:
    std::cerr << "Error: Unknown CHIP-8 opcode 0x" << std::hex << opcode
              << std::dec << " at address 0x" << std::hex
              << (cpu.program_counter - 2) << std::dec << "." << std::endl;
    return false;
  }

  return true;
}

int main(int argument_count, char *arguments[]) {
  if (argument_count > 2) {
    std::cerr << "Usage: chip_8_emulator [path/to/rom.ch8]" << std::endl;
    return 1;
  }

  CPU cpu;
  MemoryBus memory_bus;

  // Copy the 0-F font data into reserved low memory.
  for (std::size_t font_byte_index = 0; font_byte_index < font_sprites.size();
       ++font_byte_index) {
    memory_bus.memory[font_byte_index] = font_sprites[font_byte_index];
  }

  SDLFrontend frontend;
  if (!frontend.initialize()) {
    return 1;
  }

  SDL_Color pixel_color = {120, 255, 150, 255};
  std::string rom_file_path;
  if (argument_count == 2) {
    rom_file_path = arguments[1];
  } else {
    const LauncherResult launcher_result = run_launcher(
        frontend.renderer(), frontend.window(), rom_file_path, pixel_color);
    if (launcher_result == LauncherResult::quit) {
      return 0;
    }
    if (launcher_result == LauncherResult::error) {
      return 1;
    }
  }

  if (!frontend.prepare_game_display()) {
    return 1;
  }

  if (!load_rom(memory_bus, rom_file_path)) {
    return 1;
  }

  using Clock = std::chrono::steady_clock;
  using Seconds = std::chrono::duration<double>;
  constexpr Seconds opcode_period{1.0 / kInstructionsPerSecond};
  constexpr Seconds timer_period{1.0 / kTimerUpdatesPerSecond};
  constexpr Seconds maximum_frame_time{0.1};

  auto previous_frame_time = Clock::now();
  Seconds opcode_accumulator{0};
  Seconds timer_accumulator{0};

  while (frontend.poll_events(memory_bus)) {
    const auto current_frame_time = Clock::now();
    Seconds elapsed_frame_time = current_frame_time - previous_frame_time;
    previous_frame_time = current_frame_time;

    // Avoid a long catch-up burst if the window was paused or dragged.
    if (elapsed_frame_time > maximum_frame_time) {
      elapsed_frame_time = maximum_frame_time;
    }

    opcode_accumulator += elapsed_frame_time;
    timer_accumulator += elapsed_frame_time;

    // Run instructions at a steady rate instead of as fast as the CPU allows.
    while (opcode_accumulator >= opcode_period) {
      uint16_t opcode;
      if (!fetch_next_opcode(cpu, memory_bus, opcode)) {
        return 1;
      }

      if (!execute_opcode(cpu, memory_bus, opcode)) {
        return 1;
      }

      opcode_accumulator -= opcode_period;
    }

    // CHIP-8 timers tick at 60 Hz, independently of the instruction rate.
    while (timer_accumulator >= timer_period) {
      if (cpu.delay_timer > 0) {
        --cpu.delay_timer;
      }
      if (cpu.sound_timer > 0) {
        --cpu.sound_timer;
      }
      timer_accumulator -= timer_period;
    }

    if (!frontend.update_sound(cpu.sound_timer)) {
      return 1;
    }

    if (!frontend.render(memory_bus, pixel_color)) {
      return 1;
    }
    frontend.wait_for_next_frame();
  }

  return 0;
}