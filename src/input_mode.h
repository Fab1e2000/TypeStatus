#pragma once

#include <windows.h>

namespace typestatus {

enum class InputMode {
    unknown,
    chinese,
    english,
};

struct InputModeSample {
    InputMode mode = InputMode::unknown;
    const wchar_t* reason = L"";
    DWORD process_id = 0;
    DWORD thread_id = 0;
    HKL keyboard_layout = nullptr;
};

class InputModeDetector {
public:
    [[nodiscard]] InputModeSample Sample(DWORD message_timeout_ms = 100) const noexcept;
};

[[nodiscard]] const wchar_t* InputModeName(InputMode mode) noexcept;

}  // namespace typestatus
