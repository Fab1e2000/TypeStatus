#include "app.h"
#include "watchdog.h"

#include <windows.h>

#include <string>

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE /*previous_instance*/,
    PWSTR /*command_line*/,
    int show_command) {
    DWORD parent_process_id = 0;
    if (typestatus::ParseWatchdogCommandLine(parent_process_id)) {
        return typestatus::RunWatchdog(parent_process_id);
    }

    HANDLE instance_mutex = CreateMutexW(
        nullptr,
        TRUE,
        L"Local\\TypeStatus.MVP.SingleInstance");
    if (instance_mutex == nullptr) {
        MessageBoxW(
            nullptr,
            L"无法创建单实例锁。",
            L"TypeStatus",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(
            nullptr,
            L"TypeStatus 已经在运行，请检查系统托盘。",
            L"TypeStatus",
            MB_OK | MB_ICONINFORMATION);
        CloseHandle(instance_mutex);
        return 0;
    }

    HANDLE watchdog_clean_exit_event = nullptr;
    std::wstring watchdog_error;
    if (!typestatus::StartWatchdog(
            watchdog_clean_exit_event,
            watchdog_error)) {
        MessageBoxW(
            nullptr,
            watchdog_error.c_str(),
            L"TypeStatus",
            MB_OK | MB_ICONERROR);
        ReleaseMutex(instance_mutex);
        CloseHandle(instance_mutex);
        return 1;
    }

    typestatus::App app;
    const int result = app.Run(instance, show_command);
    typestatus::SignalWatchdogCleanExit(watchdog_clean_exit_event);
    ReleaseMutex(instance_mutex);
    CloseHandle(instance_mutex);
    return result;
}
