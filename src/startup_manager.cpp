#include "startup_manager.h"

#include <windows.h>

#include <string>
#include <vector>

namespace typestatus {
namespace {

constexpr wchar_t kRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kRunValueName[] = L"TypeStatus";

std::wstring BuildRegistryErrorMessage(LSTATUS status) {
    return L"无法更新开机启动设置。（Windows 错误 " +
           std::to_wstring(status) + L"）";
}

bool GetExecutableCommand(std::wstring& command) noexcept {
    std::wstring executable_path(32768, L'\0');
    const DWORD path_length = GetModuleFileNameW(
        nullptr,
        executable_path.data(),
        static_cast<DWORD>(executable_path.size()));
    if (path_length == 0 ||
        path_length >= static_cast<DWORD>(executable_path.size())) {
        return false;
    }

    executable_path.resize(path_length);
    command = L"\"" + executable_path + L"\"";
    return true;
}

bool ReadStartupCommand(std::wstring& command) noexcept {
    DWORD bytes = 0;
    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        kRunValueName,
        RRF_RT_REG_SZ,
        nullptr,
        nullptr,
        &bytes);
    if (status != ERROR_SUCCESS || bytes < sizeof(wchar_t)) {
        return false;
    }

    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t), L'\0');
    status = RegGetValueW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        kRunValueName,
        RRF_RT_REG_SZ,
        nullptr,
        buffer.data(),
        &bytes);
    if (status != ERROR_SUCCESS) {
        return false;
    }

    command.assign(buffer.data());
    return true;
}

}  // namespace

StartupState detail::DetermineStartupState(
    bool configured,
    std::wstring_view configured_command,
    std::wstring_view current_command) noexcept {
    if (!configured) {
        return StartupState::disabled;
    }
    if (current_command.empty()) {
        return StartupState::stale;
    }

    return CompareStringOrdinal(
               configured_command.data(),
               static_cast<int>(configured_command.size()),
               current_command.data(),
               static_cast<int>(current_command.size()),
               TRUE) == CSTR_EQUAL
        ? StartupState::enabled
        : StartupState::stale;
}

StartupState GetStartupState() noexcept {
    std::wstring configured_command;
    const bool configured = ReadStartupCommand(configured_command);
    std::wstring current_command;
    GetExecutableCommand(current_command);
    return detail::DetermineStartupState(
        configured,
        configured_command,
        current_command);
}

bool SetStartupEnabled(bool enabled, std::wstring& error_message) {
    HKEY run_key = nullptr;
    LSTATUS status = enabled
        ? RegCreateKeyExW(
              HKEY_CURRENT_USER,
              kRunKeyPath,
              0,
              nullptr,
              REG_OPTION_NON_VOLATILE,
              KEY_SET_VALUE,
              nullptr,
              &run_key,
              nullptr)
        : RegOpenKeyExW(
              HKEY_CURRENT_USER,
              kRunKeyPath,
              0,
              KEY_SET_VALUE,
              &run_key);
    if (status != ERROR_SUCCESS) {
        if (!enabled && status == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        error_message = BuildRegistryErrorMessage(status);
        return false;
    }

    if (enabled) {
        std::wstring command;
        if (!GetExecutableCommand(command)) {
            status = GetLastError();
            if (status == ERROR_SUCCESS) {
                status = ERROR_INSUFFICIENT_BUFFER;
            }
        } else {
            status = RegSetValueExW(
                run_key,
                kRunValueName,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        }
    } else {
        status = RegDeleteValueW(run_key, kRunValueName);
        if (status == ERROR_FILE_NOT_FOUND) {
            status = ERROR_SUCCESS;
        }
    }

    RegCloseKey(run_key);
    if (status != ERROR_SUCCESS) {
        error_message = BuildRegistryErrorMessage(status);
        return false;
    }
    return true;
}

}  // namespace typestatus
