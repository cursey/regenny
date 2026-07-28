cmake_minimum_required(VERSION 3.28)
project(regenny)
set(CMAKE_CXX_STANDARD 23)
include(cmake/CPM.cmake)

add_subdirectory(third_party)

#
# regenny
#
file(GLOB_RECURSE regenny_sources "src/*")
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
        httplib
        Zydis
)

#
# Copy the agent navigation guide next to the executable so the /api/help
# endpoint (which resolves AGENT.md relative to the working directory) can
# find it when ReGenny is launched from its build/output directory.
#
add_custom_command(TARGET regenny POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/AGENT.md"
        "$<TARGET_FILE_DIR:regenny>/AGENT.md"
        VERBATIM)