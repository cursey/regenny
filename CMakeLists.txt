cmake_minimum_required(VERSION 3.28)
project(regenny)
set(CMAKE_CXX_STANDARD 23)
include(cmake/CPM.cmake)

add_subdirectory(third_party)

#
# regenny
#
file(GLOB_RECURSE regenny_sources "src/*")

if(MSVC)
    list(FILTER regenny_sources EXCLUDE REGEX "src/arch/Linux.cpp$")
else()
    list(FILTER regenny_sources EXCLUDE REGEX "src/arch/Windows.cpp$")
endif()

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
        SDL3::SDL3
        sdkgenny::sdkgenny
        glad
        scope_guard
        SDL_Trigger
        lua
        sol2::sol2
        luagenny
)
