#pragma once

#include <string>
#include <string_view>

namespace typestatus {

enum class StartupState {
    disabled,
    enabled,
    stale,
};

[[nodiscard]] StartupState GetStartupState() noexcept;
bool SetStartupEnabled(bool enabled, std::wstring& error_message);

namespace detail {

[[nodiscard]] StartupState DetermineStartupState(
    bool configured,
    std::wstring_view configured_command,
    std::wstring_view current_command) noexcept;

}  // namespace detail

}  // namespace typestatus
