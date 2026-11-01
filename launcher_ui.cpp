#include "launcher_ui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {

constexpr int kLauncherWidth = 1000;
constexpr int kLauncherHeight = 700;
constexpr int kListTop = 178;
constexpr int kListRowHeight = 36;
constexpr int kVisibleEntries = 10;

struct ColorChoice {
  const char *name;
  SDL_Color color;
};

constexpr std::array<ColorChoice, 10> kColorChoices = {{
    {"GREEN", {120, 255, 150, 255}},
    {"CYAN", {80, 230, 255, 255}},
    {"BLUE", {100, 145, 255, 255}},
    {"PURPLE", {185, 120, 255, 255}},
    {"MAGENTA", {255, 110, 220, 255}},
    {"RED", {255, 105, 105, 255}},
    {"ORANGE", {255, 170, 80, 255}},
    {"YELLOW", {255, 230, 100, 255}},
    {"WHITE", {245, 245, 245, 255}},
    {"LIME", {190, 255, 80, 255}},
}};

struct FileEntry {
  std::filesystem::path path;
  bool is_directory = false;
};

struct LauncherState {
  std::filesystem::path directory;
  std::vector<FileEntry> entries;
  std::string selected_rom;
  std::string message;
  std::size_t selected_entry = 0;
  std::size_t first_visible_entry = 0;
  bool has_selected_entry = false;
  SDL_Color pixel_color = kColorChoices[0].color;
};

SDL_Color rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return {red, green, blue, 255};
}

void set_draw_color(SDL_Renderer *renderer, SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

void fill_rect(SDL_Renderer *renderer, const SDL_Rect &rectangle,
               SDL_Color color) {
  set_draw_color(renderer, color);
  SDL_RenderFillRect(renderer, &rectangle);
}

void draw_rect(SDL_Renderer *renderer, const SDL_Rect &rectangle,
               SDL_Color color) {
  set_draw_color(renderer, color);
  SDL_RenderDrawRect(renderer, &rectangle);
}

std::array<uint8_t, 7> glyph_for(char character) {
  switch (
      static_cast<char>(std::toupper(static_cast<unsigned char>(character)))) {
  case 'A':
    return {14, 17, 17, 31, 17, 17, 17};
  case 'B':
    return {30, 17, 17, 30, 17, 17, 30};
  case 'C':
    return {14, 17, 16, 16, 16, 17, 14};
  case 'D':
    return {30, 17, 17, 17, 17, 17, 30};
  case 'E':
    return {31, 16, 16, 30, 16, 16, 31};
  case 'F':
    return {31, 16, 16, 30, 16, 16, 16};
  case 'G':
    return {14, 17, 16, 23, 17, 17, 15};
  case 'H':
    return {17, 17, 17, 31, 17, 17, 17};
  case 'I':
    return {14, 4, 4, 4, 4, 4, 14};
  case 'J':
    return {7, 2, 2, 2, 18, 18, 12};
  case 'K':
    return {17, 18, 20, 24, 20, 18, 17};
  case 'L':
    return {16, 16, 16, 16, 16, 16, 31};
  case 'M':
    return {17, 27, 21, 21, 17, 17, 17};
  case 'N':
    return {17, 25, 21, 19, 17, 17, 17};
  case 'O':
    return {14, 17, 17, 17, 17, 17, 14};
  case 'P':
    return {30, 17, 17, 30, 16, 16, 16};
  case 'Q':
    return {14, 17, 17, 17, 21, 18, 13};
  case 'R':
    return {30, 17, 17, 30, 20, 18, 17};
  case 'S':
    return {15, 16, 16, 14, 1, 1, 30};
  case 'T':
    return {31, 4, 4, 4, 4, 4, 4};
  case 'U':
    return {17, 17, 17, 17, 17, 17, 14};
  case 'V':
    return {17, 17, 17, 17, 17, 10, 4};
  case 'W':
    return {17, 17, 17, 21, 21, 21, 10};
  case 'X':
    return {17, 17, 10, 4, 10, 17, 17};
  case 'Y':
    return {17, 17, 10, 4, 4, 4, 4};
  case 'Z':
    return {31, 1, 2, 4, 8, 16, 31};
  case '0':
    return {14, 17, 19, 21, 25, 17, 14};
  case '1':
    return {4, 12, 4, 4, 4, 4, 14};
  case '2':
    return {14, 17, 1, 2, 4, 8, 31};
  case '3':
    return {30, 1, 1, 14, 1, 1, 30};
  case '4':
    return {2, 6, 10, 18, 31, 2, 2};
  case '5':
    return {31, 16, 16, 30, 1, 1, 30};
  case '6':
    return {14, 16, 16, 30, 17, 17, 14};
  case '7':
    return {31, 1, 2, 4, 8, 8, 8};
  case '8':
    return {14, 17, 17, 14, 17, 17, 14};
  case '9':
    return {14, 17, 17, 15, 1, 1, 14};
  case '.':
    return {0, 0, 0, 0, 0, 12, 12};
  case '-':
    return {0, 0, 0, 31, 0, 0, 0};
  case '_':
    return {0, 0, 0, 0, 0, 0, 31};
  case '/':
    return {1, 1, 2, 4, 8, 16, 16};
  case '\\':
    return {16, 16, 8, 4, 2, 1, 1};
  case ':':
    return {0, 12, 12, 0, 12, 12, 0};
  case '[':
    return {14, 8, 8, 8, 8, 8, 14};
  case ']':
    return {14, 2, 2, 2, 2, 2, 14};
  case '(':
    return {2, 4, 8, 8, 8, 4, 2};
  case ')':
    return {8, 4, 2, 2, 2, 4, 8};
  case '+':
    return {0, 4, 4, 31, 4, 4, 0};
  case ' ':
    return {0, 0, 0, 0, 0, 0, 0};
  default:
    return {31, 17, 2, 4, 4, 0, 4};
  }
}

void draw_text(SDL_Renderer *renderer, int x, int y, const std::string &text,
               int scale, SDL_Color color, int maximum_width) {
  set_draw_color(renderer, color);
  int cursor_x = x;

  for (char character : text) {
    if (cursor_x + 5 * scale > x + maximum_width) {
      break;
    }

    const auto glyph = glyph_for(character);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((glyph[row] & (1 << (4 - column))) == 0) {
          continue;
        }
        SDL_Rect pixel = {cursor_x + column * scale, y + row * scale, scale,
                          scale};
        SDL_RenderFillRect(renderer, &pixel);
      }
    }
    cursor_x += 6 * scale;
  }
}

bool is_rom_file(const std::filesystem::path &path) {
  std::string extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return extension == ".ch8" || extension == ".chip8" || extension == ".c8" ||
         extension == ".rom";
}

bool refresh_entries(LauncherState &state) {
  state.entries.clear();
  state.selected_entry = 0;
  state.first_visible_entry = 0;
  state.has_selected_entry = false;
  state.message.clear();

  std::error_code error;
  std::filesystem::directory_iterator iterator(state.directory, error);
  if (error) {
    state.message = "CANNOT READ THIS FOLDER";
    std::cerr << "Error: Could not browse " << state.directory << ": "
              << error.message() << std::endl;
    return false;
  }

  const std::filesystem::directory_iterator end;
  while (iterator != end) {
    const auto &entry = *iterator;
    std::error_code entry_error;
    const bool is_directory = entry.is_directory(entry_error);
    if (!entry_error && (is_directory || is_rom_file(entry.path()))) {
      state.entries.push_back({entry.path(), is_directory});
    }

    iterator.increment(error);
    if (error) {
      state.message = "ERROR WHILE READING FOLDER";
      std::cerr << "Error: Could not continue browsing " << state.directory
                << ": " << error.message() << std::endl;
      break;
    }
  }

  std::sort(state.entries.begin(), state.entries.end(),
            [](const FileEntry &left, const FileEntry &right) {
              if (left.is_directory != right.is_directory) {
                return left.is_directory;
              }
              return left.path.filename().string() <
                     right.path.filename().string();
            });

  if (state.entries.empty()) {
    state.message = "NO ROMS OR FOLDERS HERE";
  }
  return true;
}

bool contains(const SDL_Rect &rectangle, int x, int y) {
  return x >= rectangle.x && y >= rectangle.y &&
         x < rectangle.x + rectangle.w && y < rectangle.y + rectangle.h;
}

void draw_launcher(SDL_Renderer *renderer, const LauncherState &state) {
  fill_rect(renderer, {0, 0, kLauncherWidth, kLauncherHeight}, rgb(15, 19, 27));
  draw_text(renderer, 32, 24, "CHIP-8 EMULATOR", 3, rgb(235, 241, 250), 520);
  draw_text(renderer, 36, 57, "CHOOSE A ROM AND PIXEL COLOR", 1,
            rgb(145, 159, 181), 550);

  const SDL_Rect file_panel = {28, 96, 574, 520};
  const SDL_Rect color_panel = {620, 96, 352, 520};
  fill_rect(renderer, file_panel, rgb(25, 31, 42));
  fill_rect(renderer, color_panel, rgb(25, 31, 42));
  draw_rect(renderer, file_panel, rgb(55, 66, 83));
  draw_rect(renderer, color_panel, rgb(55, 66, 83));

  const SDL_Rect up_button = {38, 112, 90, 42};
  fill_rect(renderer, up_button, rgb(48, 59, 76));
  draw_text(renderer, 58, 126, "UP", 2, rgb(225, 232, 242), 60);
  draw_text(renderer, 145, 126, state.directory.string(), 1, rgb(173, 187, 207),
            430);
  draw_text(renderer, 40, 160, "FOLDERS AND ROM FILES", 1, rgb(145, 159, 181),
            540);

  for (int row = 0; row < kVisibleEntries; ++row) {
    const std::size_t entry_index =
        state.first_visible_entry + static_cast<std::size_t>(row);
    if (entry_index >= state.entries.size()) {
      break;
    }

    const int y = kListTop + row * kListRowHeight;
    const bool selected =
        state.has_selected_entry && entry_index == state.selected_entry;
    const SDL_Rect row_rectangle = {38, y, 554, kListRowHeight - 2};
    fill_rect(renderer, row_rectangle,
              selected ? rgb(51, 72, 96) : rgb(31, 39, 52));
    const FileEntry &entry = state.entries[entry_index];
    draw_text(
        renderer, 48, y + 10,
        (entry.is_directory ? "[+] " : "    ") + entry.path.filename().string(),
        2, entry.is_directory ? rgb(123, 194, 255) : rgb(220, 228, 239), 530);
  }

  if (!state.selected_rom.empty()) {
    draw_text(renderer, 40, 548, "ROM:", 1, rgb(145, 159, 181), 45);
    draw_text(renderer, 82, 548, state.selected_rom, 1, rgb(220, 228, 239),
              500);
  } else {
    draw_text(renderer, 40, 548,
              state.message.empty() ? "SELECT A ROM FILE" : state.message, 1,
              rgb(255, 190, 110), 540);
  }

  const SDL_Rect launch_button = {38, 568, 180, 38};
  const bool can_launch = !state.selected_rom.empty();
  fill_rect(renderer, launch_button,
            can_launch ? rgb(65, 145, 98) : rgb(55, 63, 74));
  draw_text(renderer, 62, 580, "LAUNCH", 2,
            can_launch ? rgb(245, 255, 248) : rgb(145, 151, 160), 140);

  draw_text(renderer, 640, 116, "PIXEL COLOR", 2, rgb(235, 241, 250), 300);
  for (std::size_t index = 0; index < kColorChoices.size(); ++index) {
    const int column = static_cast<int>(index % 2);
    const int row = static_cast<int>(index / 2);
    const SDL_Rect swatch = {640 + column * 158, 151 + row * 42, 145, 34};
    const SDL_Color color = kColorChoices[index].color;
    fill_rect(renderer, swatch, rgb(37, 45, 59));
    fill_rect(renderer, {swatch.x + 7, swatch.y + 6, 22, 22}, color);
    draw_text(renderer, swatch.x + 38, swatch.y + 12, kColorChoices[index].name,
              1, rgb(220, 228, 239), 100);
    if (state.pixel_color.r == color.r && state.pixel_color.g == color.g &&
        state.pixel_color.b == color.b) {
      draw_rect(renderer, swatch, rgb(245, 247, 252));
    } else {
      draw_rect(renderer, swatch, rgb(61, 72, 89));
    }
  }

  draw_text(renderer, 640, 378, "CUSTOM RGB", 1, rgb(145, 159, 181), 180);
  const std::array<uint8_t, 3> channels = {
      state.pixel_color.r, state.pixel_color.g, state.pixel_color.b};
  const std::array<const char *, 3> channel_names = {"R", "G", "B"};
  const std::array<SDL_Color, 3> channel_colors = {
      rgb(255, 105, 105), rgb(120, 230, 145), rgb(110, 155, 255)};

  for (std::size_t channel = 0; channel < channels.size(); ++channel) {
    const int y = 410 + static_cast<int>(channel) * 48;
    draw_text(renderer, 640, y + 4, channel_names[channel], 1,
              channel_colors[channel], 20);
    const SDL_Rect track = {665, y + 5, 194, 12};
    fill_rect(renderer, track, rgb(54, 64, 80));
    const int thumb_x =
        track.x + static_cast<int>(channels[channel]) * (track.w - 8) / 255;
    fill_rect(renderer, {thumb_x, track.y - 4, 8, 20}, channel_colors[channel]);
    draw_text(renderer, 870, y + 3, std::to_string(channels[channel]), 1,
              rgb(220, 228, 239), 50);
  }

  draw_text(renderer, 640, 566, "PREVIEW", 1, rgb(145, 159, 181), 100);
  fill_rect(renderer, {740, 558, 205, 38}, state.pixel_color);
  draw_rect(renderer, {740, 558, 205, 38}, rgb(235, 241, 250));
  draw_text(renderer, 38, 650,
            "ESC: QUIT   CLICK A ROM TO SELECT   CLICK A COLOR OR RGB SLIDER",
            1, rgb(130, 145, 166), 920);
}

void change_rgb_channel(LauncherState &state, int mouse_x,
                        std::size_t channel) {
  constexpr int track_x = 665;
  constexpr int track_width = 194;
  const int value =
      std::clamp((mouse_x - track_x) * 255 / (track_width - 8), 0, 255);
  if (channel == 0) {
    state.pixel_color.r = static_cast<uint8_t>(value);
  } else if (channel == 1) {
    state.pixel_color.g = static_cast<uint8_t>(value);
  } else {
    state.pixel_color.b = static_cast<uint8_t>(value);
  }
}

void move_selection(LauncherState &state, int direction) {
  if (state.entries.empty()) {
    return;
  }
  if (!state.has_selected_entry) {
    state.selected_entry = 0;
    state.has_selected_entry = true;
  } else if (direction < 0 && state.selected_entry > 0) {
    --state.selected_entry;
  } else if (direction > 0 && state.selected_entry + 1 < state.entries.size()) {
    ++state.selected_entry;
  }

  if (state.selected_entry < state.first_visible_entry) {
    state.first_visible_entry = state.selected_entry;
  } else if (state.selected_entry >=
             state.first_visible_entry + kVisibleEntries) {
    state.first_visible_entry = state.selected_entry - kVisibleEntries + 1;
  }
}

void activate_selection(LauncherState &state) {
  if (!state.has_selected_entry ||
      state.selected_entry >= state.entries.size()) {
    return;
  }

  const FileEntry &entry = state.entries[state.selected_entry];
  if (entry.is_directory) {
    state.directory = entry.path;
    state.selected_rom.clear();
    refresh_entries(state);
    return;
  }

  state.selected_rom = entry.path.string();
  state.message.clear();
}

} // namespace

LauncherResult run_launcher(SDL_Renderer *renderer, SDL_Window *window,
                            std::string &rom_file_path,
                            SDL_Color &pixel_color) {
  LauncherState state;
  char *executable_directory = SDL_GetBasePath();
  if (executable_directory == nullptr) {
    std::cerr << "Error: Could not determine the executable directory: "
              << SDL_GetError() << std::endl;
    return LauncherResult::error;
  }
  state.directory = executable_directory;
  SDL_free(executable_directory);

  if (!refresh_entries(state)) {
    state.message = "CANNOT READ START FOLDER";
  }
  for (std::size_t index = 0; index < state.entries.size(); ++index) {
    if (!state.entries[index].is_directory &&
        state.entries[index].path.filename() == "pong.ch8") {
      state.selected_entry = index;
      state.has_selected_entry = true;
      state.selected_rom = state.entries[index].path.string();
      if (index >= kVisibleEntries) {
        state.first_visible_entry = index - kVisibleEntries + 1;
      }
      break;
    }
  }

  SDL_SetWindowTitle(window, "CHIP-8 Emulator - Select ROM and Pixel Color");
  SDL_SetWindowSize(window, kLauncherWidth, kLauncherHeight);
  if (SDL_RenderSetLogicalSize(renderer, kLauncherWidth, kLauncherHeight) !=
      0) {
    std::cerr << "Error: Could not configure the launcher display: "
              << SDL_GetError() << std::endl;
    return LauncherResult::error;
  }
  SDL_StartTextInput();

  bool keep_running = true;
  LauncherResult result = LauncherResult::quit;
  while (keep_running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        keep_running = false;
      } else if (event.type == SDL_MOUSEWHEEL) {
        const int direction = event.wheel.y > 0 ? -1 : 1;
        for (int step = 0; step < std::abs(event.wheel.y); ++step) {
          move_selection(state, direction);
        }
      } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          keep_running = false;
        } else if (event.key.keysym.sym == SDLK_UP) {
          move_selection(state, -1);
        } else if (event.key.keysym.sym == SDLK_DOWN) {
          move_selection(state, 1);
        } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
          const std::filesystem::path parent = state.directory.parent_path();
          if (!parent.empty() && parent != state.directory) {
            state.directory = parent;
            state.selected_rom.clear();
            refresh_entries(state);
          }
        } else if (event.key.keysym.sym == SDLK_RETURN) {
          activate_selection(state);
          if (!state.selected_rom.empty()) {
            rom_file_path = state.selected_rom;
            pixel_color = state.pixel_color;
            result = LauncherResult::launch;
            keep_running = false;
          }
        }
      } else if (event.type == SDL_MOUSEBUTTONDOWN &&
                 event.button.button == SDL_BUTTON_LEFT) {
        const int mouse_x = event.button.x;
        const int mouse_y = event.button.y;
        const SDL_Rect up_button = {38, 112, 90, 42};
        const SDL_Rect launch_button = {38, 568, 180, 38};

        if (contains(up_button, mouse_x, mouse_y)) {
          const std::filesystem::path parent = state.directory.parent_path();
          if (!parent.empty() && parent != state.directory) {
            state.directory = parent;
            state.selected_rom.clear();
            refresh_entries(state);
          }
        } else if (contains(launch_button, mouse_x, mouse_y) &&
                   !state.selected_rom.empty()) {
          rom_file_path = state.selected_rom;
          pixel_color = state.pixel_color;
          result = LauncherResult::launch;
          keep_running = false;
        } else if (mouse_x >= 38 && mouse_x < 592 && mouse_y >= kListTop &&
                   mouse_y < kListTop + kVisibleEntries * kListRowHeight) {
          const std::size_t clicked_entry =
              state.first_visible_entry +
              static_cast<std::size_t>((mouse_y - kListTop) / kListRowHeight);
          if (clicked_entry < state.entries.size()) {
            state.selected_entry = clicked_entry;
            state.has_selected_entry = true;
            if (state.entries[clicked_entry].is_directory) {
              state.directory = state.entries[clicked_entry].path;
              state.selected_rom.clear();
              refresh_entries(state);
            } else {
              state.selected_rom = state.entries[clicked_entry].path.string();
              state.message.clear();
            }
          }
        } else {
          bool selected_color = false;
          for (std::size_t index = 0; index < kColorChoices.size(); ++index) {
            const int column = static_cast<int>(index % 2);
            const int row = static_cast<int>(index / 2);
            const SDL_Rect swatch = {640 + column * 158, 151 + row * 42, 145,
                                     34};
            if (contains(swatch, mouse_x, mouse_y)) {
              state.pixel_color = kColorChoices[index].color;
              selected_color = true;
              break;
            }
          }

          if (!selected_color && mouse_x >= 640 && mouse_x <= 860) {
            for (std::size_t channel = 0; channel < 3; ++channel) {
              const int slider_y = 410 + static_cast<int>(channel) * 48;
              if (mouse_y >= slider_y && mouse_y <= slider_y + 24) {
                change_rgb_channel(state, mouse_x, channel);
                selected_color = true;
                break;
              }
            }
          }
        }
      } else if (event.type == SDL_MOUSEMOTION &&
                 (event.motion.state & SDL_BUTTON_LMASK) != 0 &&
                 event.motion.x >= 640 && event.motion.x <= 860) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
          const int slider_y = 410 + static_cast<int>(channel) * 48;
          if (event.motion.y >= slider_y && event.motion.y <= slider_y + 24) {
            change_rgb_channel(state, event.motion.x, channel);
            break;
          }
        }
      }
    }

    draw_launcher(renderer, state);
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
  }

  SDL_StopTextInput();
  return result;
}
