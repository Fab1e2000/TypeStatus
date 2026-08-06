#include "cursor_renderer.h"

#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace typestatus {
namespace {

constexpr UINT kSystemArrowId = 32512;
constexpr UINT kSystemIBeamId = 32513;
constexpr UINT kRestoreSystemCursors = 0x0057;

const Gdiplus::Color kChineseCursorColor(255, 229, 72, 77);
const Gdiplus::Color kEnglishCursorColor(255, 59, 130, 246);

class BitmapHandle {
public:
    BitmapHandle() = default;
    explicit BitmapHandle(HBITMAP value) : value_(value) {}
    BitmapHandle(const BitmapHandle&) = delete;
    BitmapHandle& operator=(const BitmapHandle&) = delete;
    ~BitmapHandle() {
        if (value_ != nullptr) {
            DeleteObject(value_);
        }
    }

private:
    HBITMAP value_ = nullptr;
};

class CursorHandle {
public:
    CursorHandle() = default;
    explicit CursorHandle(HCURSOR value) : value_(value) {}
    CursorHandle(const CursorHandle&) = delete;
    CursorHandle& operator=(const CursorHandle&) = delete;
    ~CursorHandle() {
        if (value_ != nullptr) {
            DestroyCursor(value_);
        }
    }

    [[nodiscard]] HCURSOR get() const noexcept { return value_; }
    [[nodiscard]] HCURSOR release() noexcept {
        const HCURSOR result = value_;
        value_ = nullptr;
        return result;
    }

private:
    HCURSOR value_ = nullptr;
};

class IconHandle {
public:
    explicit IconHandle(HICON value) : value_(value) {}
    IconHandle(const IconHandle&) = delete;
    IconHandle& operator=(const IconHandle&) = delete;
    ~IconHandle() {
        if (value_ != nullptr) {
            DestroyIcon(value_);
        }
    }

private:
    HICON value_ = nullptr;
};

std::runtime_error Win32Failure(const char* operation) {
    std::ostringstream message;
    message << operation << " failed (Win32 error " << GetLastError() << ")";
    return std::runtime_error(message.str());
}

std::runtime_error GdiPlusFailure(
    const char* operation,
    Gdiplus::Status status) {
    std::ostringstream message;
    message << operation << " failed (GDI+ status "
            << static_cast<int>(status) << ")";
    return std::runtime_error(message.str());
}

std::wstring ToWide(const std::exception& error) {
    const std::string text = error.what();
    if (text.empty()) {
        return L"Unknown error";
    }

    const int length = MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (length <= 0) {
        return L"Cursor operation failed";
    }

    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        length);
    return result;
}

void CheckStatus(const char* operation, Gdiplus::Status status) {
    if (status != Gdiplus::Ok) {
        throw GdiPlusFailure(operation, status);
    }
}

void RenderOnBackground(
    HCURSOR cursor,
    int width,
    int height,
    const Gdiplus::Color& background,
    Gdiplus::Bitmap& output) {
    Gdiplus::Graphics graphics(&output);
    CheckStatus("Graphics construction", graphics.GetLastStatus());
    CheckStatus("Graphics::Clear", graphics.Clear(background));

    HDC device_context = graphics.GetHDC();
    if (device_context == nullptr) {
        throw std::runtime_error("Graphics::GetHDC failed");
    }

    const BOOL drawn = DrawIconEx(
        device_context,
        0,
        0,
        cursor,
        width,
        height,
        0,
        nullptr,
        DI_NORMAL);
    graphics.ReleaseHDC(device_context);

    if (drawn == FALSE) {
        throw Win32Failure("DrawIconEx");
    }
}

int GetCoverage(
    const Gdiplus::Color& on_black,
    const Gdiplus::Color& on_white) noexcept {
    const int red_delta = on_white.GetR() - on_black.GetR();
    const int green_delta = on_white.GetG() - on_black.GetG();
    const int blue_delta = on_white.GetB() - on_black.GetB();

    if (red_delta < 0 || green_delta < 0 || blue_delta < 0) {
        return 255;
    }

    const int transmission = (red_delta + green_delta + blue_delta) / 3;
    return std::clamp(255 - transmission, 0, 255);
}

bool IsInvertedPixel(
    const Gdiplus::Color& on_black,
    const Gdiplus::Color& on_white) noexcept {
    return on_white.GetR() < on_black.GetR() ||
           on_white.GetG() < on_black.GetG() ||
           on_white.GetB() < on_black.GetB();
}

Gdiplus::Color RecoverSourceColor(
    const Gdiplus::Color& on_black,
    int coverage) noexcept {
    if (coverage <= 0) {
        return Gdiplus::Color(0, 0, 0, 0);
    }

    const auto recover = [coverage](BYTE value) {
        return static_cast<BYTE>(std::min(255, value * 255 / coverage));
    };
    return Gdiplus::Color(
        255,
        recover(on_black.GetR()),
        recover(on_black.GetG()),
        recover(on_black.GetB()));
}

Gdiplus::Color TintByLuminance(
    const Gdiplus::Color& source,
    const Gdiplus::Color& target) noexcept {
    const int luminance =
        (source.GetR() * 2126 +
         source.GetG() * 7152 +
         source.GetB() * 722) /
        10000;

    const auto tint_channel = [luminance](BYTE target_channel) {
        const int distance_to_white = 255 - target_channel;
        return static_cast<BYTE>(
            target_channel + (distance_to_white * luminance + 127) / 255);
    };

    return Gdiplus::Color(
        255,
        tint_channel(target.GetR()),
        tint_channel(target.GetG()),
        tint_channel(target.GetB()));
}

HCURSOR CreateColorVariant(
    HCURSOR source,
    const Gdiplus::Color& target_color,
    bool preserve_source_luminance) {
    ICONINFO source_info{};
    if (GetIconInfo(source, &source_info) == FALSE) {
        throw Win32Failure("GetIconInfo");
    }
    BitmapHandle source_mask(source_info.hbmMask);
    BitmapHandle source_color(source_info.hbmColor);

    const HBITMAP size_bitmap = source_info.hbmColor != nullptr
        ? source_info.hbmColor
        : source_info.hbmMask;
    BITMAP bitmap_info{};
    if (size_bitmap == nullptr ||
        GetObjectW(size_bitmap, sizeof(bitmap_info), &bitmap_info) == 0) {
        throw Win32Failure("GetObjectW");
    }

    const int width = std::abs(bitmap_info.bmWidth);
    int height = std::abs(bitmap_info.bmHeight);
    if (source_info.hbmColor == nullptr) {
        height /= 2;
    }
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("System cursor has invalid dimensions");
    }

    Gdiplus::Bitmap on_black(width, height, PixelFormat32bppARGB);
    Gdiplus::Bitmap on_white(width, height, PixelFormat32bppARGB);
    Gdiplus::Bitmap colored(width, height, PixelFormat32bppARGB);
    CheckStatus("Black bitmap construction", on_black.GetLastStatus());
    CheckStatus("White bitmap construction", on_white.GetLastStatus());
    CheckStatus("Colored bitmap construction", colored.GetLastStatus());

    RenderOnBackground(
        source,
        width,
        height,
        Gdiplus::Color(255, 0, 0, 0),
        on_black);
    RenderOnBackground(
        source,
        width,
        height,
        Gdiplus::Color(255, 255, 255, 255),
        on_white);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Gdiplus::Color black_pixel;
            Gdiplus::Color white_pixel;
            CheckStatus("Bitmap::GetPixel (black)", on_black.GetPixel(x, y, &black_pixel));
            CheckStatus("Bitmap::GetPixel (white)", on_white.GetPixel(x, y, &white_pixel));

            const int coverage = GetCoverage(black_pixel, white_pixel);
            const Gdiplus::Color output =
                !preserve_source_luminance ||
                IsInvertedPixel(black_pixel, white_pixel)
                ? target_color
                : TintByLuminance(
                      RecoverSourceColor(black_pixel, coverage),
                      target_color);

            const Gdiplus::Color pixel(
                static_cast<BYTE>(coverage),
                output.GetR(),
                output.GetG(),
                output.GetB());
            CheckStatus("Bitmap::SetPixel", colored.SetPixel(x, y, pixel));
        }
    }

    HICON temporary_icon = nullptr;
    CheckStatus("Bitmap::GetHICON", colored.GetHICON(&temporary_icon));
    IconHandle temporary_icon_guard(temporary_icon);

    ICONINFO colored_info{};
    if (GetIconInfo(temporary_icon, &colored_info) == FALSE) {
        throw Win32Failure("GetIconInfo (colored cursor)");
    }
    BitmapHandle colored_mask(colored_info.hbmMask);
    BitmapHandle colored_color(colored_info.hbmColor);

    colored_info.fIcon = FALSE;
    colored_info.xHotspot = source_info.xHotspot;
    colored_info.yHotspot = source_info.yHotspot;
    HCURSOR result = reinterpret_cast<HCURSOR>(
        CreateIconIndirect(&colored_info));
    if (result == nullptr) {
        throw Win32Failure("CreateIconIndirect");
    }
    return result;
}

void DestroyAndClear(HCURSOR& cursor) noexcept {
    if (cursor != nullptr) {
        DestroyCursor(cursor);
        cursor = nullptr;
    }
}

void ApplyCursor(HCURSOR variant, DWORD system_cursor_id) {
    if (variant == nullptr) {
        throw std::runtime_error("Cursor variants are not initialized");
    }

    CursorHandle copy(reinterpret_cast<HCURSOR>(
        CopyIcon(reinterpret_cast<HICON>(variant))));
    if (copy.get() == nullptr) {
        throw Win32Failure("CopyIcon");
    }

    // SetSystemCursor owns and destroys the supplied handle after success.
    if (SetSystemCursor(copy.get(), system_cursor_id) == FALSE) {
        throw Win32Failure("SetSystemCursor");
    }
    static_cast<void>(copy.release());
}

}  // namespace

CursorRenderer::~CursorRenderer() {
    Shutdown();
}

bool CursorRenderer::Initialize(std::wstring& error_message) noexcept {
    try {
        Shutdown();
        RestoreSystemCursors();

        Gdiplus::GdiplusStartupInput startup_input;
        const Gdiplus::Status startup_status = Gdiplus::GdiplusStartup(
            &gdiplus_token_,
            &startup_input,
            nullptr);
        CheckStatus("GdiplusStartup", startup_status);

        const HCURSOR source_arrow = LoadCursorW(nullptr, IDC_ARROW);
        if (source_arrow == nullptr) {
            throw Win32Failure("LoadCursorW (arrow)");
        }
        const HCURSOR source_ibeam = LoadCursorW(nullptr, IDC_IBEAM);
        if (source_ibeam == nullptr) {
            throw Win32Failure("LoadCursorW (I-beam)");
        }

        chinese_ibeam_ = CreateColorVariant(
            source_ibeam,
            kChineseCursorColor,
            false);
        english_ibeam_ = CreateColorVariant(
            source_ibeam,
            kEnglishCursorColor,
            false);
        chinese_arrow_ = CreateColorVariant(
            source_arrow,
            kChineseCursorColor,
            true);
        english_arrow_ = CreateColorVariant(
            source_arrow,
            kEnglishCursorColor,
            true);
        return true;
    } catch (const std::exception& error) {
        error_message = ToWide(error);
        RestoreSystemCursors();
        Shutdown();
        return false;
    }
}

bool CursorRenderer::Apply(
    InputMode mode,
    std::wstring& error_message) noexcept {
    try {
        switch (mode) {
            case InputMode::chinese:
                ApplyCursor(chinese_arrow_, kSystemArrowId);
                ApplyCursor(chinese_ibeam_, kSystemIBeamId);
                break;
            case InputMode::english:
                ApplyCursor(english_arrow_, kSystemArrowId);
                ApplyCursor(english_ibeam_, kSystemIBeamId);
                break;
            default:
                RestoreSystemCursors();
                break;
        }
        return true;
    } catch (const std::exception& error) {
        error_message = ToWide(error);
        RestoreSystemCursors();
        return false;
    }
}

void CursorRenderer::RestoreSystemCursors() noexcept {
    SystemParametersInfoW(
        kRestoreSystemCursors,
        0,
        nullptr,
        0);
}

void CursorRenderer::Shutdown() noexcept {
    DisposeVariants();
    if (gdiplus_token_ != 0) {
        Gdiplus::GdiplusShutdown(gdiplus_token_);
        gdiplus_token_ = 0;
    }
}

void CursorRenderer::DisposeVariants() noexcept {
    DestroyAndClear(chinese_ibeam_);
    DestroyAndClear(english_ibeam_);
    DestroyAndClear(chinese_arrow_);
    DestroyAndClear(english_arrow_);
}

}  // namespace typestatus
