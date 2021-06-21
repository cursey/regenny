cmake_minimum_required(VERSION 3.5)

project(ReGenny)

#include(cmake/FindThor.cmake)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)

find_package(freetype CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SFML COMPONENTS system window graphics CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(pegtl CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Thor 2 REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)

add_executable(regenny 
    "thirdparty/imgui/imgui_freetype.cpp"
    "thirdparty/imgui/imgui_freetype.h"
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
    "src/Module.hpp" 
    "src/Utility.hpp" 
    "src/Utility.cpp" 
    "src/MemoryUi.hpp" 
    "src/MemoryUi.cpp" 
    "src/node/Base.hpp" 
    "src/node/Base.cpp" 
    "src/node/Variable.hpp" 
    "src/node/Variable.cpp" 
    "src/node/Array.hpp" 
    "src/node/Array.cpp" 
    "src/node/Struct.hpp" 
    "src/node/Struct.cpp" 
    "src/node/Pointer.hpp" 
    "src/node/Pointer.cpp" 
    "src/node/Undefined.hpp" 
    "src/node/Undefined.cpp" 
    "src/LoggerUi.hpp" 
    "src/LoggerUi.cpp" 
    "src/node/Bitfield.hpp" 
    "src/node/Bitfield.cpp" 
    "src/node/Property.hpp")
target_compile_features(regenny PRIVATE cxx_std_17)
target_include_directories(regenny PRIVATE "src/" "thirdparty/imgui/" ${THOR_INCLUDE_DIR})
target_link_libraries(regenny PRIVATE 
    freetype
    imgui::imgui 
    sfml-system sfml-network sfml-graphics sfml-window sfml-main
    ImGui-SFML::ImGui-SFML
    fmt::fmt
    taocpp::pegtl
    unofficial::nativefiledialog::nfd
    spdlog::spdlog
    spdlog::spdlog_header_only
    ${THOR_LIBRARY}
    utf8cpp)
target_compile_definitions(regenny PRIVATE GENNY_INCLUDE_PARSER UTF_CPP_CPLUSPLUS=201103L)
