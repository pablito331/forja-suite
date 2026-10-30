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
add_subdirectory(Brasa)
add_subdirectory(Tempera)
add_subdirectory(Prensa)
add_subdirectory(Malho)
add_subdirectory(Eq)
add_subdirectory(Dynamics)
add_subdirectory(Strips)
add_subdirectory(Tape)
add_subdirectory(Space)
add_subdirectory(Lofi)
add_subdirectory(OneKnob)
add_subdirectory(Amps)
add_subdirectory(Tests)
