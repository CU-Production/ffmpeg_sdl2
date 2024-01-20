cmake_minimum_required(VERSION 3.27)
project(ffmpeg_sdl2)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(3rd_party)

add_executable(tutorial01 tutorial01/tutorial01.cpp)
target_link_libraries(tutorial01 FFmpeg SDL2285 stb)
add_custom_command(
        TARGET tutorial01 PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/ffmpeg-master-latest-win64-gpl-shared/bin "${CMAKE_CURRENT_BINARY_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/SDL2-2.28.5/lib/x64 "${CMAKE_CURRENT_BINARY_DIR}/"
        VERBATIM
)

add_executable(tutorial02 tutorial02/tutorial02.cpp)
target_link_libraries(tutorial02 FFmpeg SDL2285)
add_custom_command(
        TARGET tutorial02 PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/ffmpeg-master-latest-win64-gpl-shared/bin "${CMAKE_CURRENT_BINARY_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/SDL2-2.28.5/lib/x64 "${CMAKE_CURRENT_BINARY_DIR}/"
        VERBATIM
)

add_executable(tutorial03 tutorial03/tutorial03.cpp)
target_link_libraries(tutorial03 FFmpeg SDL2285)
add_custom_command(
        TARGET tutorial03 PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/ffmpeg-master-latest-win64-gpl-shared/bin "${CMAKE_CURRENT_BINARY_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different  ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/SDL2-2.28.5/lib/x64 "${CMAKE_CURRENT_BINARY_DIR}/"
        VERBATIM
)
