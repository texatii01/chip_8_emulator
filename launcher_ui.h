#ifndef CHIP_8_LAUNCHER_UI_H
#define CHIP_8_LAUNCHER_UI_H

#include <string>

#include <SDL.h>

enum class LauncherResult { launch, quit, error };

LauncherResult run_launcher(SDL_Renderer *renderer, SDL_Window *window,
                            std::string &rom_file_path, SDL_Color &pixel_color);

#endif
