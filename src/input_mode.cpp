#include "input_mode.h"

#include <imm.h>

namespace typestatus {
namespace {

constexpr DWORD kImeNativeMode = 0x0001;
constexpr WPARAM kGetConversionMode = 0x0001;
constexpr WPARAM kGetOpenStatus = 0x0005;
constexpr UINT kSendFlags =
    SMTO_BLOCK | SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT;

struct ImeQueryResult {
    bool succeeded = false;
    DWORD_PTR value = 0;
};

ImeQueryResult QueryImeControl(
    HWND ime_window,
    WPARAM command,
    DWORD timeout_ms) noexcept {
    ImeQueryResult query;
    if (ime_window == nullptr) {
        return query;
    }

    query.succeeded = SendMessageTimeoutW(
        ime_window,
        WM_IME_CONTROL,
        command,
        0,
        kSendFlags,
        timeout_ms,
        &query.value) != 0;
    return query;
}

}  // namespace

InputMode detail::DetermineInputMode(
    LANGID language_id,
    bool open_status_succeeded,
    DWORD_PTR open_status,
    bool conversion_mode_succeeded,
    DWORD_PTR conversion_mode) noexcept {
    if (PRIMARYLANGID(language_id) == LANG_ENGLISH) {
        return InputMode::english;
    }
    if (open_status_succeeded && open_status == 0) {
        return InputMode::english;
    }
    if (!conversion_mode_succeeded) {
        return InputMode::unknown;
    }
    return (conversion_mode & kImeNativeMode) != 0
        ? InputMode::chinese
        : InputMode::english;
}

InputModeSample InputModeDetector::Sample(DWORD message_timeout_ms) const noexcept {
    InputModeSample sample;

    const HWND foreground_window = GetForegroundWindow();
    if (foreground_window == nullptr) {
        sample.reason = L"No foreground window";
        return sample;
    }

    DWORD foreground_process_id = 0;
    const DWORD foreground_thread_id = GetWindowThreadProcessId(
        foreground_window,
        &foreground_process_id);
    if (foreground_thread_id == 0) {
        sample.reason = L"No foreground thread";
        return sample;
    }

    GUITHREADINFO gui_info{};
    gui_info.cbSize = sizeof(gui_info);
    const bool got_gui_info = GetGUIThreadInfo(
        foreground_thread_id,
        &gui_info) != FALSE;
    const HWND focus_window = got_gui_info && gui_info.hwndFocus != nullptr
        ? gui_info.hwndFocus
        : foreground_window;

    DWORD focus_process_id = 0;
    DWORD focus_thread_id = GetWindowThreadProcessId(
        focus_window,
        &focus_process_id);
    if (focus_thread_id == 0) {
        focus_thread_id = foreground_thread_id;
        focus_process_id = foreground_process_id;
    }

    sample.process_id = focus_process_id;
    sample.thread_id = focus_thread_id;
    sample.keyboard_layout = GetKeyboardLayout(focus_thread_id);

    const auto raw_layout = reinterpret_cast<ULONG_PTR>(sample.keyboard_layout);
    const LANGID language_id = LOWORD(raw_layout);
    if (PRIMARYLANGID(language_id) == LANG_ENGLISH) {
        sample.mode = detail::DetermineInputMode(
            language_id,
            false,
            0,
            false,
            0);
        sample.reason = L"English-family keyboard layout";
        return sample;
    }

    HWND ime_window = ImmGetDefaultIMEWnd(focus_window);
    if (ime_window == nullptr && focus_window != foreground_window) {
        ime_window = ImmGetDefaultIMEWnd(foreground_window);
    }

    const ImeQueryResult open_status = QueryImeControl(
        ime_window,
        kGetOpenStatus,
        message_timeout_ms);
    if (open_status.succeeded && open_status.value == 0) {
        sample.mode = detail::DetermineInputMode(
            language_id,
            open_status.succeeded,
            open_status.value,
            false,
            0);
        sample.reason = L"IME reports closed";
        return sample;
    }

    const ImeQueryResult conversion_mode = QueryImeControl(
        ime_window,
        kGetConversionMode,
        message_timeout_ms);
    sample.mode = detail::DetermineInputMode(
        language_id,
        open_status.succeeded,
        open_status.value,
        conversion_mode.succeeded,
        conversion_mode.value);
    if (sample.mode == InputMode::chinese) {
        sample.reason = L"IME native conversion mode is enabled";
    } else if (sample.mode == InputMode::english) {
        sample.reason = L"IME native conversion mode is disabled";
    } else {
        sample.reason = L"IME status query failed";
    }

    return sample;
}

const wchar_t* InputModeName(InputMode mode) noexcept {
    switch (mode) {
        case InputMode::chinese:
            return L"中文";
        case InputMode::english:
            return L"英文";
        default:
            return L"未知";
    }
}

}  // namespace typestatus
