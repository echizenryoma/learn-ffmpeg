cmake_minimum_required(VERSION 3.8)

project("learn-ffmpeg")
link_libraries("-static")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

set(CMAKE_LEGACY_CYGWIN_WIN32 0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(fmt CONFIG REQUIRED)

find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
if(FFMPEG_FOUND)
    message("FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES}")
    message("SWSCALE_LIBRARIES: ${SWSCALE_LIBRARIES}")
    include_directories(${FFMPEG_INCLUDE_DIRS})
endif()

GET_DIRECTORY_PROPERTY(STB_SEARCH_PATHS INCLUDE_DIRECTORIES)
message("STB_SEARCH_PATHS: ${STB_SEARCH_PATHS}")
find_path(STB_INCLUDE_DIRS 
    NAMES
		stb.h
    PATHS
        ${STB_SEARCH_PATHS}
    PATH_SUFFIXES
        include
        stb
)
message("STB_INCLUDE_DIRS: ${STB_INCLUDE_DIRS}")
include_directories(${STB_INCLUDE_DIRS})

file(GLOB SOURCES "src/*.h" "src/*.cpp" "src/*.hpp")
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    fmt::fmt-header-only
    ${FFMPEG_LIBRARIES}
    ${SWSCALE_LIBRARIES}
)