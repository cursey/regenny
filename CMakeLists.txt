cmake_minimum_required(VERSION 3.5)

project(ReGenny)

find_package(imgui CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(SFML COMPONENTS system window graphics CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)

add_executable(regenny 
    "src/Genny.hpp" 
    "src/Main.cpp" 
    "src/ReGenny.hpp" 
    "src/ReGenny.cpp" 
    "src/Process.hpp" 
    "src/arch/Windows.hpp" 
    "src/arch/Windows.cpp" 
    "src/Helpers.hpp" 
    "src/arch/Arch.hpp" 
    "src/arch/Arch.cpp" 
    "src/Module.hpp")
target_compile_features(regenny PRIVATE cxx_std_17)
target_include_directories(regenny PRIVATE "src/")
target_link_libraries(regenny PRIVATE 
    imgui::imgui 
    spdlog::spdlog spdlog::spdlog_header_only 
    sfml-system sfml-network sfml-graphics sfml-window sfml-main
    ImGui-SFML::ImGui-SFML
    fmt::fmt
    sol2::sol2)
