cmake_minimum_required(VERSION 3.8)

project("learn-ffmpeg")
link_libraries("-static")

set(CMAKE_LEGACY_CYGWIN_WIN32 0)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(fmt CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil libswscale)

find_package(OpenCV REQUIRED)

file(GLOB SOURCES "src/*.h" "src/*.cpp" "src/*.hpp")
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_directories(${PROJECT_NAME} PRIVATE ${OpenCV_LIBRARY_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS} fmt::fmt-header-only PkgConfig::ffmpeg)