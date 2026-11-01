cmake_minimum_required(VERSION 3.16)

project(chip_8_emulator VERSION 1.0.1 LANGUAGES CXX)

find_package(SDL2 CONFIG REQUIRED)

add_executable(chip_8_emulator main.cpp)
target_compile_features(chip_8_emulator PRIVATE cxx_std_17)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pong.ch8")
    add_custom_command(
        TARGET chip_8_emulator POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/pong.ch8"
                "$<TARGET_FILE_DIR:chip_8_emulator>/pong.ch8"
        COMMENT "Copying Pong ROM next to the emulator"
    )
endif()

if(WIN32 AND TARGET SDL2::SDL2main)
    target_link_libraries(chip_8_emulator PRIVATE SDL2::SDL2main SDL2::SDL2)
else()
    target_link_libraries(chip_8_emulator PRIVATE SDL2::SDL2)
endif()
