cmake_minimum_required(VERSION 3.7)
set(CMAKE_CXX_STANDARD 17)


project(texteditor)
add_executable(texteditor "src/main.cpp")

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/SDL2/cmake/)
find_package(SDL2 REQUIRED)

list(APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/SDL2_ttf/cmake/)
find_package(SDL2_ttf REQUIRED)

target_link_libraries(texteditor PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)