cmake_minimum_required(VERSION 3.24)

project(TypeStatus VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "TypeStatus is a Windows-only application.")
endif()

add_executable(TypeStatus WIN32
    src/app.cpp
    src/app.h
    src/cursor_renderer.cpp
    src/cursor_renderer.h
    src/input_mode.cpp
    src/input_mode.h
    src/main.cpp
    resources/app.rc
    resources/app.manifest
    resources/resource.h
)

target_compile_features(TypeStatus PRIVATE cxx_std_20)
target_compile_definitions(TypeStatus PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_include_directories(TypeStatus PRIVATE resources)

if(MSVC)
    target_compile_options(TypeStatus PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_link_options(TypeStatus PRIVATE /MANIFEST:NO)
    set_property(TARGET TypeStatus PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(TypeStatus PRIVATE
    user32
    shell32
    gdi32
    imm32
    gdiplus
)

install(TARGETS TypeStatus RUNTIME DESTINATION .)
