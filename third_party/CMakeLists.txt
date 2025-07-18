# imgui
CPMAddPackage(
        NAME imgui
        GITHUB_REPOSITORY ocornut/imgui
        VERSION 1.91.9b-docking
        DOWNLOAD_ONLY YES
)
if (imgui_ADDED)
    file(GLOB imgui_sources ${imgui_SOURCE_DIR}/*.cpp)
    add_library(imgui STATIC
            ${imgui_sources}
            ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
            ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)
    target_include_directories(imgui PUBLIC
            $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
            $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/backends>
            $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/misc/cpp>)
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif ()

# fmt
CPMAddPackage("gh:fmtlib/fmt#11.0.2")

# pegtl
CPMAddPackage("gh:taocpp/pegtl#3.2.7")

# nativefiledialog
CPMAddPackage(
        NAME nativefiledialog
        GITHUB_REPOSITORY mlabbe/nativefiledialog
        VERSION 1.16
        GIT_TAG release_116
        DOWNLOAD_ONLY YES
)
if (nativefiledialog_ADDED)
    add_library(
            nativefiledialog STATIC
            ${nativefiledialog_SOURCE_DIR}/src/nfd_common.c
            ${nativefiledialog_SOURCE_DIR}/src/nfd_win.cpp
    )
    target_include_directories(nativefiledialog PUBLIC $<BUILD_INTERFACE:${nativefiledialog_SOURCE_DIR}>/src/include)
endif ()

# spdlog
CPMAddPackage(
        NAME spdlog
        GITHUB_REPOSITORY gabime/spdlog
        VERSION 1.14.1
        OPTIONS
        "SPDLOG_FMT_EXTERNAL ON")

# utfcpp
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

# nlohmann_json
CPMAddPackage("gh:nlohmann/json@3.11.3")

# SDL3
CPMAddPackage(
        NAME SDL3
        GITHUB_REPOSITORY libsdl-org/SDL
        VERSION 3.2.14
        GIT_TAG release-3.2.14
        OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST_LIBRARY OFF"
)

# sdkgenny
CPMAddPackage("gh:cursey/sdkgenny#cbf986af49c45afb2016c96202b66d66fa1cbd8e")

# Lua
CPMAddPackage(
        NAME lua
        GITHUB_REPOSITORY lua/lua
        GIT_TAG v5.4.7
        DOWNLOAD_ONLY TRUE
)
if (lua_ADDED)
    file(GLOB lua_sources ${lua_SOURCE_DIR}/*.c)
    list(REMOVE_ITEM lua_sources ${lua_SOURCE_DIR}/lua.c ${lua_SOURCE_DIR}/luac.c ${lua_SOURCE_DIR}/onelua.c)
    add_library(lua STATIC ${lua_sources})
    target_include_directories(lua PUBLIC $<BUILD_INTERFACE:${lua_SOURCE_DIR}>)
endif ()

# sol2
CPMAddPackage("gh:ThePhD/sol2@3.3.0")

# luagenny
CPMAddPackage(
        NAME luagenny
        GITHUB_REPOSITORY praydog/luagenny
        GIT_TAG 30e204780387a665a6d43d41619dc990413925c1
        DOWNLOAD_ONLY YES
)
if (luagenny_ADDED)
    file(GLOB_RECURSE luagenny_sources ${luagenny_SOURCE_DIR}/src/*.cpp)
    add_library(luagenny STATIC ${luagenny_sources})
    target_include_directories(luagenny PUBLIC ${luagenny_SOURCE_DIR}/include)
    target_link_libraries(luagenny PUBLIC lua sol2::sol2 sdkgenny::sdkgenny taocpp::pegtl)
    if (MSVC)
        target_compile_options(luagenny PUBLIC /bigobj)
    else ()
        target_compile_options(luagenny PRIVATE -fdeclspec -Wno-ignored-attributes)
    endif ()
endif ()

# Glad
add_library(glad STATIC "glad/src/glad.c")
target_include_directories(glad PUBLIC "glad/include")

# scope_guard
add_library(scope_guard INTERFACE)
target_include_directories(scope_guard INTERFACE "scope_guard")

# SDL_Trigger
add_library(SDL_Trigger STATIC "SDL_Trigger/SDL_Trigger.cpp")
target_include_directories(SDL_Trigger INTERFACE "SDL_Trigger")
target_link_libraries(SDL_Trigger PUBLIC SDL3::SDL3)
