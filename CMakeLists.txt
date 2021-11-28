cmake_minimum_required(VERSION 3.8)

project("learn-ffmpeg")
# link_libraries("-static")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

set(CMAKE_LEGACY_CYGWIN_WIN32 0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(fmt CONFIG)
link_libraries(fmt::fmt fmt::fmt-header-only)

find_package(spdlog CONFIG REQUIRED)
link_libraries(spdlog::spdlog spdlog::spdlog_header_only)

find_package(SDL2 CONFIG REQUIRED)
link_libraries(SDL2::SDL2 SDL2::SDL2main)

find_package(FFMPEG REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE SWRESAMPLE)
message("FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES}")
include_directories(${FFMPEG_INCLUDE_DIRS})
link_libraries(${FFMPEG_LIBRARIES})

find_package(STB REQUIRED)
message("STB_INCLUDE_DIRS: ${STB_INCLUDE_DIRS}")
include_directories(${STB_INCLUDE_DIRS})

file(GLOB SOURCES "src/*.cpp" "src/*.hpp")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")
add_executable(${PROJECT_NAME} ${SOURCES})
