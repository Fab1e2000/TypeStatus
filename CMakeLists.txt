cmake_minimum_required(VERSION 3.24)

project(TypeStatus VERSION 1.0.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "TypeStatus is a Windows-only application.")
endif()

add_executable(TypeStatus WIN32
    resources/app-icon.ico
    resources/app-icon.svg
    src/color_math.h
    src/app.cpp
    src/app.h
    src/cursor_renderer.cpp
    src/cursor_renderer.h
    src/input_mode.cpp
    src/input_mode.h
    src/main.cpp
    src/startup_manager.cpp
    src/startup_manager.h
    src/watchdog.cpp
    src/watchdog.h
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
    advapi32
    user32
    shell32
    gdi32
    imm32
    gdiplus
)

install(TARGETS TypeStatus RUNTIME DESTINATION .)

include(CTest)

if(BUILD_TESTING)
    add_executable(TypeStatusCoreTests
        tests/core_tests.cpp
        src/color_math.h
        src/input_mode.cpp
        src/input_mode.h
        src/startup_manager.cpp
        src/startup_manager.h
    )
    target_compile_features(TypeStatusCoreTests PRIVATE cxx_std_20)
    target_compile_definitions(TypeStatusCoreTests PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_include_directories(TypeStatusCoreTests PRIVATE src)
    if(MSVC)
        target_compile_options(TypeStatusCoreTests PRIVATE /W4 /permissive- /utf-8 /EHsc)
        set_property(TARGET TypeStatusCoreTests PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    target_link_libraries(TypeStatusCoreTests PRIVATE advapi32 user32 imm32)
    add_test(NAME TypeStatusCoreTests COMMAND TypeStatusCoreTests)
endif()
