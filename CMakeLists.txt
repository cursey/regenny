cmake_minimum_required(VERSION 3.28)
project(regenny)

# Set project wide C++ standard
set(CMAKE_CXX_STANDARD 23)

#
# Dependencies
#
include(cmake/CPM.cmake)
CPMAddPackage(
        NAME imgui
        GITHUB_REPOSITORY ocornut/imgui
        VERSION 1.90.9-docking
        DOWNLOAD_ONLY YES)

if (imgui_ADDED)
    file(GLOB imgui_sources ${imgui_SOURCE_DIR}/*.cpp)
    add_library(imgui STATIC
            ${imgui_sources}
            ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
            ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)
    target_include_directories(imgui PUBLIC
            $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
            $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/backends>
            $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/misc/cpp>)
    target_link_libraries(imgui PUBLIC SDL2::SDL2)
endif ()

CPMAddPackage("gh:fmtlib/fmt#11.0.2")
CPMAddPackage("gh:taocpp/pegtl#3.2.7")

CPMAddPackage(
        NAME nativefiledialog
        GITHUB_REPOSITORY mlabbe/nativefiledialog
        VERSION 1.16
        GIT_TAG release_116
        DOWNLOAD_ONLY YES)

if (nativefiledialog_ADDED)
    add_library(nativefiledialog STATIC
            ${nativefiledialog_SOURCE_DIR}/src/nfd_common.c
            ${nativefiledialog_SOURCE_DIR}/src/nfd_win.cpp)
    target_include_directories(nativefiledialog PUBLIC $<BUILD_INTERFACE:${nativefiledialog_SOURCE_DIR}>/src/include)
endif ()

CPMAddPackage(
        NAME spdlog
        GITHUB_REPOSITORY gabime/spdlog
        VERSION 1.14.1
        OPTIONS
        "SPDLOG_FMT_EXTERNAL ON")
CPMAddPackage(
        NAME utfcpp
        GITHUB_REPOSITORY nemtrif/utfcpp
        VERSION 4.0.5
        DOWNLOAD_ONLY YES)

if (utfcpp_ADDED)
    add_library(utfcpp INTERFACE)
    target_include_directories(utfcpp INTERFACE $<BUILD_INTERFACE:${utfcpp_SOURCE_DIR}/source>)
    target_compile_definitions(utfcpp INTERFACE UTF_CPP_CPLUSPLUS=202002L)
endif ()

CPMAddPackage("gh:nlohmann/json@3.11.3")
CPMAddPackage(
        NAME SDL2
        GITHUB_REPOSITORY libsdl-org/SDL
        VERSION 2.30.5
        GIT_TAG release-2.30.5
        OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON")
CPMAddPackage("gh:cursey/sdkgenny#cbf986af49c45afb2016c96202b66d66fa1cbd8e")

#
# Third party
#

# Glad
add_library(glad STATIC "thirdparty/glad/src/glad.c")
target_include_directories(glad PUBLIC "thirdparty/glad/include")

# scope_guard
add_library(scope_guard INTERFACE)
target_include_directories(scope_guard INTERFACE "thirdparty/scope_guard")

# SDL_Trigger
add_library(SDL_Trigger STATIC "thirdparty/SDL_Trigger/SDL_Trigger.cpp")
target_include_directories(SDL_Trigger INTERFACE "thirdparty/SDL_Trigger")
target_link_libraries(SDL_Trigger PUBLIC SDL2::SDL2)

#
# regenny
#
file(GLOB_RECURSE regenny_sources "src/**")
add_executable(regenny ${regenny_sources})
target_include_directories(regenny PRIVATE "src")
target_link_libraries(regenny PRIVATE
        imgui
        fmt::fmt
        taocpp::pegtl
        nativefiledialog
        spdlog::spdlog
        utfcpp
        nlohmann_json::nlohmann_json
        SDL2::SDL2
        SDL2::SDL2main
        sdkgenny::sdkgenny
        glad
        scope_guard
        SDL_Trigger)