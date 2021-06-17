cmake_minimum_required(VERSION 3.17)
INCLUDE_DIRECTORIES( ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR} )

project(LaserBombon)

file(GLOB all_SRCS
    "${PROJECT_SOURCE_DIR}/EntityCore/*/*.cpp"
    "${PROJECT_SOURCE_DIR}/EntityLib/*/*.cpp"
    "${PROJECT_SOURCE_DIR}/LaserBombon/*/*.cpp"
    )

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

FIND_PACKAGE(SDL2 REQUIRED)
FIND_PACKAGE(SDL2_mixer REQUIRED)
FIND_PACKAGE(SDL2_ttf REQUIRED)
FIND_PACKAGE(Vulkan REQUIRED)

SET(CMAKE_CXX_FLAGS "-g3 -Wextra -Wall -Wno-sign-compare")
set(CPACK_SOURCE_IGNORE_FILES "/.git/" "/build/")

add_executable(LaserBombon ${all_SRCS})
target_link_libraries(LaserBombon ${SDL2_LIBRARY} ${SDL2_MIXER_LIBRARY} ${SDL2_TTF_LIBRARY} ${Vulkan_LIBRARIES})
