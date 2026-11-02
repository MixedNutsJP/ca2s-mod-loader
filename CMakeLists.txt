cmake_minimum_required(VERSION 3.20)
project(ca2s_mod_loader LANGUAGES CXX ASM_MASM)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "The game is x64 only. Configure with -A x64.")
endif()

# Static CRT, so nothing here adds a runtime dependency to the game.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

function(mixednuts_target target)
  target_compile_features(${target} PRIVATE cxx_std_17)
  target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4 /WX /utf-8>)
  target_include_directories(${target} PRIVATE common api)
endfunction()

# Game folder: dxgi.dll (forwards to System32 and starts the loader)
add_library(dxgi SHARED loader/proxy/proxy.cpp loader/proxy/stubs.asm loader/proxy/dxgi.def)
mixednuts_target(dxgi)

# MixedNuts\MixedNutsLoader.dll
add_library(MixedNutsLoader SHARED loader/core/loader.cpp)
mixednuts_target(MixedNutsLoader)

# cmake --install build --config Release --prefix dist
# lays out the files exactly as they go into the game folder.
install(TARGETS dxgi RUNTIME DESTINATION .)
install(TARGETS MixedNutsLoader RUNTIME DESTINATION MixedNuts)
install(FILES loader/package/MixedNuts/loader.ini DESTINATION MixedNuts)
install(FILES LICENSE DESTINATION MixedNuts RENAME LICENSE.txt)
install(DIRECTORY DESTINATION MixedNuts/Mods)
