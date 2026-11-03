cmake_minimum_required(VERSION 3.22)

project(ForjaSuite VERSION 0.4.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ForjaSuite.cmake)

enable_testing()

add_subdirectory(Forja)
add_subdirectory(Airwindows)
add_subdirectory(drumforge)
add_subdirectory(Forjarack)
add_subdirectory(Minis)
add_subdirectory(Pianos)
add_subdirectory(Tests)
