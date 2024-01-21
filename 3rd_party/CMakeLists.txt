# ffmpeg
add_library(FFmpeg INTERFACE)
target_include_directories(FFmpeg INTERFACE ffmpeg-master-latest-win64-gpl-shared/include)
target_link_directories(FFmpeg INTERFACE ffmpeg-master-latest-win64-gpl-shared/lib)
target_link_libraries(FFmpeg INTERFACE
        avcodec
        avdevice
        avfilter
        avformat
        avutil
        swresample
        swscale)

# sdl
add_library(SDL2285 INTERFACE)
target_include_directories(SDL2285 INTERFACE SDL2-2.28.5/include)
target_link_directories(SDL2285 INTERFACE SDL2-2.28.5/lib/x64)
target_link_libraries(SDL2285 INTERFACE SDL2 SDL2main)

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb)

# sokol
add_library(sokol INTERFACE)
target_include_directories(sokol INTERFACE sokol)
