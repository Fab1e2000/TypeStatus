#pragma once

#include <windows.h>

#include <string>

namespace typestatus {

[[nodiscard]] bool ParseWatchdogCommandLine(
    DWORD& parent_process_id) noexcept;
[[nodiscard]] int RunWatchdog(DWORD parent_process_id) noexcept;
bool StartWatchdog(
    HANDLE& clean_exit_event,
    std::wstring& error_message) noexcept;
void SignalWatchdogCleanExit(HANDLE clean_exit_event) noexcept;

}  // namespace typestatus
