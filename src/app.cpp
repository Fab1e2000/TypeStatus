#include "app.h"

#include <strsafe.h>

#include <string>

namespace typestatus {
namespace {

constexpr wchar_t kWindowClassName[] = L"TypeStatus.HiddenWindow";
constexpr wchar_t kAppName[] = L"TypeStatus";

std::wstring BuildTooltip(InputMode mode, bool paused) {
    if (paused) {
        return L"TypeStatus - 已暂停（系统默认光标）";
    }

    std::wstring tooltip = L"TypeStatus - ";
    tooltip += InputModeName(mode);
    switch (mode) {
        case InputMode::chinese:
            tooltip += L"（红色）";
            break;
        case InputMode::english:
            tooltip += L"（蓝色）";
            break;
        default:
            tooltip += L"（系统默认光标）";
            break;
    }
    return tooltip;
}

}  // namespace

App::~App() {
    Cleanup();
}

int App::Run(HINSTANCE instance, int /*show_command*/) {
    instance_ = instance;
    taskbar_created_message_ = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance_;
    window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&window_class) == 0) {
        MessageBoxW(
            nullptr,
            L"无法注册后台窗口。",
            kAppName,
            MB_OK | MB_ICONERROR);
        return 1;
    }

    window_ = CreateWindowExW(
        0,
        kWindowClassName,
        kAppName,
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance_,
        this);
    if (window_ == nullptr) {
        UnregisterClassW(kWindowClassName, instance_);
        return 1;
    }

    MSG message{};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) {
            break;
        }
        if (result == -1) {
            Cleanup();
            UnregisterClassW(kWindowClassName, instance_);
            return 1;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    Cleanup();
    UnregisterClassW(kWindowClassName, instance_);
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::WindowProcedure(
    HWND window,
    UINT message,
    WPARAM w_param,
    LPARAM l_param) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
        app = static_cast<App*>(create->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }

    if (app != nullptr) {
        return app->HandleMessage(window, message, w_param, l_param);
    }
    return DefWindowProcW(window, message, w_param, l_param);
}

LRESULT App::HandleMessage(
    HWND window,
    UINT message,
    WPARAM w_param,
    LPARAM l_param) {
    if (taskbar_created_message_ != 0 && message == taskbar_created_message_) {
        tray_icon_added_ = false;
        AddTrayIcon();
        return 0;
    }

    switch (message) {
        case WM_CREATE:
            return Initialize() ? 0 : -1;

        case WM_TIMER:
            if (w_param == kPollTimerId) {
                PollInputMode();
            }
            return 0;

        case kTrayCallbackMessage: {
            const UINT notification = LOWORD(l_param);
            if (notification == WM_CONTEXTMENU || notification == WM_RBUTTONUP) {
                ShowTrayMenu();
            } else if (notification == WM_LBUTTONDBLCLK) {
                ShowAbout();
            }
            return 0;
        }

        case WM_COMMAND:
            switch (LOWORD(w_param)) {
                case kCommandRestore:
                    paused_ = !paused_;
                    if (paused_) {
                        cursor_renderer_.RestoreSystemCursors();
                    } else {
                        has_applied_mode_ = false;
                        PollInputMode();
                    }
                    UpdateTrayTooltip();
                    return 0;
                case kCommandExit:
                    DestroyWindow(window_);
                    return 0;
                default:
                    break;
            }
            break;

        case WM_QUERYENDSESSION:
            return TRUE;

        case WM_ENDSESSION:
            if (w_param != FALSE) {
                Cleanup();
            }
            return 0;

        case WM_DESTROY:
            Cleanup();
            PostQuitMessage(0);
            return 0;

        case WM_NCDESTROY:
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            window_ = nullptr;
            return DefWindowProcW(window, message, w_param, l_param);

        default:
            break;
    }

    return DefWindowProcW(window, message, w_param, l_param);
}

bool App::Initialize() {
    std::wstring error_message;
    if (!cursor_renderer_.Initialize(error_message)) {
        const std::wstring message = L"无法生成状态光标：\n\n" + error_message;
        MessageBoxW(window_, message.c_str(), kAppName, MB_OK | MB_ICONERROR);
        return false;
    }

    if (!AddTrayIcon()) {
        MessageBoxW(
            window_,
            L"无法创建系统托盘图标。",
            kAppName,
            MB_OK | MB_ICONERROR);
        return false;
    }

    if (SetTimer(window_, kPollTimerId, kPollIntervalMs, nullptr) == 0) {
        MessageBoxW(
            window_,
            L"无法启动输入状态检测定时器。",
            kAppName,
            MB_OK | MB_ICONERROR);
        return false;
    }

    PollInputMode();
    return true;
}

void App::Cleanup() noexcept {
    if (cleaned_up_) {
        return;
    }
    cleaned_up_ = true;

    if (window_ != nullptr) {
        KillTimer(window_, kPollTimerId);
    }
    RemoveTrayIcon();
    cursor_renderer_.RestoreSystemCursors();
    cursor_renderer_.Shutdown();
}

void App::PollInputMode() {
    if (paused_) {
        return;
    }

    latest_sample_ = detector_.Sample();
    if (has_applied_mode_ && latest_sample_.mode == applied_mode_) {
        return;
    }

    std::wstring error_message;
    if (!cursor_renderer_.Apply(latest_sample_.mode, error_message)) {
        const std::wstring message = L"更新系统光标失败：\n\n" + error_message;
        MessageBoxW(window_, message.c_str(), kAppName, MB_OK | MB_ICONERROR);
        DestroyWindow(window_);
        return;
    }

    applied_mode_ = latest_sample_.mode;
    has_applied_mode_ = true;
    UpdateTrayTooltip();
}

bool App::AddTrayIcon() {
    NOTIFYICONDATAW icon_data{};
    icon_data.cbSize = sizeof(icon_data);
    icon_data.hWnd = window_;
    icon_data.uID = kTrayIconId;
    icon_data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    icon_data.uCallbackMessage = kTrayCallbackMessage;
    icon_data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    StringCchCopyW(
        icon_data.szTip,
        ARRAYSIZE(icon_data.szTip),
        BuildTooltip(latest_sample_.mode, paused_).c_str());

    if (Shell_NotifyIconW(NIM_ADD, &icon_data) == FALSE) {
        return false;
    }

    icon_data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &icon_data);
    tray_icon_added_ = true;
    return true;
}

void App::RemoveTrayIcon() noexcept {
    if (!tray_icon_added_ || window_ == nullptr) {
        return;
    }

    NOTIFYICONDATAW icon_data{};
    icon_data.cbSize = sizeof(icon_data);
    icon_data.hWnd = window_;
    icon_data.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &icon_data);
    tray_icon_added_ = false;
}

void App::UpdateTrayTooltip() {
    if (!tray_icon_added_) {
        return;
    }

    NOTIFYICONDATAW icon_data{};
    icon_data.cbSize = sizeof(icon_data);
    icon_data.hWnd = window_;
    icon_data.uID = kTrayIconId;
    icon_data.uFlags = NIF_TIP | NIF_SHOWTIP;
    StringCchCopyW(
        icon_data.szTip,
        ARRAYSIZE(icon_data.szTip),
        BuildTooltip(latest_sample_.mode, paused_).c_str());
    Shell_NotifyIconW(NIM_MODIFY, &icon_data);
}

void App::ShowTrayMenu() {
    POINT cursor_position{};
    if (GetCursorPos(&cursor_position) == FALSE) {
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    std::wstring status_text = L"当前状态：";
    status_text += InputModeName(latest_sample_.mode);
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, status_text.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(
        menu,
        MF_STRING,
        kCommandRestore,
        paused_ ? L"继续状态着色" : L"暂停并恢复系统光标");
    AppendMenuW(menu, MF_STRING, kCommandExit, L"退出");

    SetForegroundWindow(window_);
    TrackPopupMenu(
        menu,
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        cursor_position.x,
        cursor_position.y,
        0,
        window_,
        nullptr);
    PostMessageW(window_, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void App::ShowAbout() const {
    MessageBoxW(
        window_,
        L"TypeStatus MVP\n\n"
        L"中文输入：红色光标\n"
        L"英文输入：蓝色光标\n"
        L"无法识别：系统默认光标\n\n"
        L"本程序不读取输入内容，不注入其他进程。",
        kAppName,
        MB_OK | MB_ICONINFORMATION);
}

}  // namespace typestatus
