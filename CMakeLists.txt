cmake_minimum_required(VERSION 3.10)
project(chatter C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

# Find ncurses
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Add subdirectories
include_directories(include)
add_subdirectory(src)

# Testing
enable_testing()

include(FetchContent)
FetchContent_Declare(
  cmocka
  GIT_REPOSITORY https://git.cryptomilk.org/projects/cmocka.git
  GIT_TAG        cmocka-1.1.5
)
FetchContent_MakeAvailable(cmocka)

add_subdirectory(tests)