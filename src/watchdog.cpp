#include "watchdog.h"

#include <shellapi.h>

#include <cerrno>
#include <cwchar>
#include <iterator>
#include <string>

namespace typestatus {
namespace {

constexpr UINT kRestoreSystemCursors = 0x0057;

std::wstring BuildCleanExitEventName(DWORD process_id) {
    return L"Local\\TypeStatus.Watchdog.CleanExit." +
           std::to_wstring(process_id);
}

std::wstring BuildWindowsErrorMessage(
    const wchar_t* operation,
    DWORD error) {
    return std::wstring(operation) + L"（Windows 错误 " +
           std::to_wstring(error) + L"）";
}

}  // namespace

bool ParseWatchdogCommandLine(DWORD& parent_process_id) noexcept {
    int argument_count = 0;
    wchar_t** arguments = CommandLineToArgvW(
        GetCommandLineW(),
        &argument_count);
    if (arguments == nullptr) {
        return false;
    }

    bool parsed = false;
    if (argument_count == 3 &&
        std::wcscmp(arguments[1], L"--watchdog") == 0) {
        wchar_t* end = nullptr;
        errno = 0;
        const unsigned long value = std::wcstoul(arguments[2], &end, 10);
        if (errno == 0 && end != arguments[2] && *end == L'\0' &&
            value > 0 && value <= MAXDWORD) {
            parent_process_id = static_cast<DWORD>(value);
            parsed = true;
        }
    }

    LocalFree(arguments);
    return parsed;
}

int RunWatchdog(DWORD parent_process_id) noexcept {
    HANDLE parent_process = OpenProcess(
        SYNCHRONIZE,
        FALSE,
        parent_process_id);
    HANDLE clean_exit_event = OpenEventW(
        SYNCHRONIZE,
        FALSE,
        BuildCleanExitEventName(parent_process_id).c_str());

    bool clean_exit = false;
    if (parent_process != nullptr && clean_exit_event != nullptr) {
        const HANDLE handles[] = {clean_exit_event, parent_process};
        clean_exit = WaitForMultipleObjects(
            static_cast<DWORD>(std::size(handles)),
            handles,
            FALSE,
            INFINITE) == WAIT_OBJECT_0;
    } else if (parent_process != nullptr) {
        WaitForSingleObject(parent_process, INFINITE);
    }

    if (clean_exit_event != nullptr) {
        CloseHandle(clean_exit_event);
    }
    if (parent_process != nullptr) {
        CloseHandle(parent_process);
    }
    if (clean_exit) {
        return 0;
    }

    return SystemParametersInfoW(
               kRestoreSystemCursors,
               0,
               nullptr,
               0) != FALSE
        ? 0
        : 1;
}

bool StartWatchdog(
    HANDLE& clean_exit_event,
    std::wstring& error_message) noexcept {
    clean_exit_event = CreateEventW(
        nullptr,
        TRUE,
        FALSE,
        BuildCleanExitEventName(GetCurrentProcessId()).c_str());
    if (clean_exit_event == nullptr) {
        error_message = BuildWindowsErrorMessage(
            L"无法创建光标恢复保护事件。",
            GetLastError());
        return false;
    }

    std::wstring executable_path(32768, L'\0');
    const DWORD path_length = GetModuleFileNameW(
        nullptr,
        executable_path.data(),
        static_cast<DWORD>(executable_path.size()));
    if (path_length == 0 ||
        path_length >= static_cast<DWORD>(executable_path.size())) {
        DWORD error = GetLastError();
        if (error == ERROR_SUCCESS) {
            error = ERROR_INSUFFICIENT_BUFFER;
        }
        error_message = BuildWindowsErrorMessage(
            L"无法获取程序路径。",
            error);
        CloseHandle(clean_exit_event);
        clean_exit_event = nullptr;
        return false;
    }
    executable_path.resize(path_length);

    std::wstring command_line = L"\"" + executable_path +
        L"\" --watchdog " + std::to_wstring(GetCurrentProcessId());

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};
    if (CreateProcessW(
            executable_path.c_str(),
            command_line.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup_info,
            &process_info) == FALSE) {
        error_message = BuildWindowsErrorMessage(
            L"无法启动光标恢复保护进程。",
            GetLastError());
        CloseHandle(clean_exit_event);
        clean_exit_event = nullptr;
        return false;
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return true;
}

void SignalWatchdogCleanExit(HANDLE clean_exit_event) noexcept {
    if (clean_exit_event != nullptr) {
        SetEvent(clean_exit_event);
        CloseHandle(clean_exit_event);
    }
}

}  // namespace typestatus
