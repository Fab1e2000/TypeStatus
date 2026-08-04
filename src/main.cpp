#include "app.h"

#include <windows.h>

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE /*previous_instance*/,
    PWSTR /*command_line*/,
    int show_command) {
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

    typestatus::App app;
    const int result = app.Run(instance, show_command);
    ReleaseMutex(instance_mutex);
    CloseHandle(instance_mutex);
    return result;
}
