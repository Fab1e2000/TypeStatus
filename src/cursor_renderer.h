#pragma once

#include "input_mode.h"

#include <windows.h>

#include <string>

namespace typestatus {

class CursorRenderer {
public:
    CursorRenderer() = default;
    CursorRenderer(const CursorRenderer&) = delete;
    CursorRenderer& operator=(const CursorRenderer&) = delete;
    ~CursorRenderer();

    bool Initialize(std::wstring& error_message) noexcept;
    bool Apply(InputMode mode, std::wstring& error_message) noexcept;
    void RestoreSystemCursors() noexcept;
    void Shutdown() noexcept;

private:
    HCURSOR chinese_ibeam_ = nullptr;
    HCURSOR english_ibeam_ = nullptr;
    HCURSOR chinese_arrow_ = nullptr;
    HCURSOR english_arrow_ = nullptr;
    ULONG_PTR gdiplus_token_ = 0;

    void DisposeVariants() noexcept;
};

}  // namespace typestatus
