#include "color_math.h"
#include "input_mode.h"
#include "startup_manager.h"

#include <windows.h>

#include <iostream>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void TestColorMath() {
    constexpr typestatus::RgbColor red{229, 72, 77};
    constexpr typestatus::RgbColor black{0, 0, 0};
    constexpr typestatus::RgbColor white{255, 255, 255};
    constexpr typestatus::RgbColor gray{128, 128, 128};

    static_assert(typestatus::TintByLuminance(black, red) == red);
    static_assert(
        typestatus::TintByLuminance(white, red) == white);

    constexpr typestatus::RgbColor tinted_gray =
        typestatus::TintByLuminance(gray, red);
    Expect(tinted_gray.red >= red.red && tinted_gray.red <= 255,
           "gray red channel is between target and white");
    Expect(tinted_gray.green > red.green && tinted_gray.green < 255,
           "gray green channel preserves a highlight");
    Expect(tinted_gray.blue > red.blue && tinted_gray.blue < 255,
           "gray blue channel preserves a highlight");
}

void TestInputModeClassification() {
    using typestatus::InputMode;
    using typestatus::detail::DetermineInputMode;

    Expect(
        DetermineInputMode(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                           false, 0, false, 0) == InputMode::english,
        "English-family layouts are English");
    Expect(
        DetermineInputMode(MAKELANGID(LANG_CHINESE, SUBLANG_DEFAULT),
                           true, 0, false, 0) == InputMode::english,
        "a closed IME is English");
    Expect(
        DetermineInputMode(MAKELANGID(LANG_CHINESE, SUBLANG_DEFAULT),
                           true, 1, true, 1) == InputMode::chinese,
        "native conversion mode is Chinese");
    Expect(
        DetermineInputMode(MAKELANGID(LANG_CHINESE, SUBLANG_DEFAULT),
                           true, 1, true, 0) == InputMode::english,
        "non-native conversion mode is English");
    Expect(
        DetermineInputMode(MAKELANGID(LANG_CHINESE, SUBLANG_DEFAULT),
                           false, 0, false, 0) == InputMode::unknown,
        "an unavailable IME query is unknown");
}

void TestStartupState() {
    using typestatus::StartupState;
    using typestatus::detail::DetermineStartupState;

    Expect(
        DetermineStartupState(false, L"", L"\"C:\\TypeStatus.exe\"") ==
            StartupState::disabled,
        "a missing startup value is disabled");
    Expect(
        DetermineStartupState(
            true,
            L"\"C:\\TypeStatus.exe\"",
            L"\"c:\\typestatus.exe\"") == StartupState::enabled,
        "startup paths are compared case-insensitively");
    Expect(
        DetermineStartupState(
            true,
            L"\"C:\\Old\\TypeStatus.exe\"",
            L"\"C:\\New\\TypeStatus.exe\"") == StartupState::stale,
        "a moved executable produces a stale startup state");
}

}  // namespace

int main() {
    TestColorMath();
    TestInputModeClassification();
    TestStartupState();
    if (failures == 0) {
        std::cout << "All TypeStatus core tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
