cmake_minimum_required(VERSION 3.27)
project(ffmpeg_sdl2)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(3rd_party)

add_executable(ffmpeg_sdl2 main.cpp)
target_link_libraries(ffmpeg_sdl2 FFmpeg SDL2285 stb)
add_custom_command(
        TARGET ffmpeg_sdl2 PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/ffmpeg-master-latest-win64-gpl-shared/bin "${CMAKE_CURRENT_BINARY_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/SDL2-2.28.5/lib/x64 "${CMAKE_CURRENT_BINARY_DIR}/"
        VERBATIM
)

add_executable(tutorial01 tutorial01/tutorial01.cpp)
target_link_libraries(tutorial01 FFmpeg SDL2285 stb)
add_custom_command(
        TARGET tutorial01 PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/ffmpeg-master-latest-win64-gpl-shared/bin "${CMAKE_CURRENT_BINARY_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/SDL2-2.28.5/lib/x64 "${CMAKE_CURRENT_BINARY_DIR}/"
        VERBATIM
)
