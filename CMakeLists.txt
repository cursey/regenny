cmake_minimum_required(VERSION 3.16)

project(ReGenny)

find_package(imgui CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(pegtl CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)

if(MSVC)
    set(EXE_TYPE WIN32)
endif()

add_executable(regenny ${EXE_TYPE} 
    "src/Genny.hpp" 
    "src/Main.cpp" 
    "src/ReGenny.hpp" 
    "src/ReGenny.cpp" 
    "src/Process.hpp" 
    "src/arch/Windows.hpp" 
    "src/arch/Windows.cpp" 
    "src/arch/Windows.rc"
    "src/Helpers.hpp" 
    "src/arch/Arch.hpp" 
    "src/arch/Arch.cpp" 
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
    "src/node/Property.hpp" 
    "src/AboutUi.hpp" 
    "src/AboutUi.cpp" 
    "thirdparty/SDL_Trigger/sdl_trigger.cpp")
target_compile_features(regenny PRIVATE cxx_std_17)
target_include_directories(regenny PRIVATE "src/" "thirdparty/scope_gaurd/" "thirdparty/SDL_Trigger/")
target_link_libraries(regenny PRIVATE 
    imgui::imgui 
    fmt::fmt
    taocpp::pegtl
    unofficial::nativefiledialog::nfd
    spdlog::spdlog
    spdlog::spdlog_header_only
    utf8cpp
    SDL2::SDL2
    SDL2::SDL2main
    tomlplusplus::tomlplusplus)
target_compile_definitions(regenny PRIVATE 
    GENNY_INCLUDE_PARSER # The SdkGenny parser is required
    UTF_CPP_CPLUSPLUS=201103L # utf8cpp doesn't properly recognize modern msvc
    TOML_WINDOWS_COMPAT=0 # toml++ linker errors when compiled w/ compatability enabled
    )
set_target_properties(regenny 
    PROPERTIES 
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    if(${VCPKG_TARGET_TRIPLET} MATCHES ".+static")
        set_target_properties(regenny
            PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

macro(regenny_add_test test_name test_files)
    add_executable(${test_name} ${test_files})
    set_target_properties(${test_name}
        PROPERTIES 
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    if(MSVC)
        if(${VCPKG_TARGET_TRIPLET} MATCHES ".+static")
            set_target_properties(${test_name}
                PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
    endif()
endmacro()

regenny_add_test(test0 "examples/test0.cpp")
