#pragma once

#include "cursor_renderer.h"
#include "input_mode.h"

#include <windows.h>
#include <shellapi.h>

namespace typestatus {

class App {
public:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    ~App();

    int Run(HINSTANCE instance, int show_command);

private:
    static constexpr UINT_PTR kPollTimerId = 1;
    static constexpr UINT kPollIntervalMs = 200;
    static constexpr UINT kTrayCallbackMessage = WM_APP + 1;
    static constexpr UINT kTrayIconId = 1;
    static constexpr UINT kCommandRestore = 1001;
    static constexpr UINT kCommandStartup = 1002;
    static constexpr UINT kCommandExit = 1003;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT taskbar_created_message_ = 0;
    bool tray_icon_added_ = false;
    bool cleaned_up_ = false;
    bool paused_ = false;
    bool has_applied_mode_ = false;
    InputMode applied_mode_ = InputMode::unknown;
    InputModeSample latest_sample_;
    InputModeDetector detector_;
    CursorRenderer cursor_renderer_;

    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM w_param,
        LPARAM l_param);
    LRESULT HandleMessage(
        HWND window,
        UINT message,
        WPARAM w_param,
        LPARAM l_param);

    bool Initialize();
    void Cleanup() noexcept;
    void PollInputMode();
    bool AddTrayIcon();
    void RemoveTrayIcon() noexcept;
    void UpdateTrayTooltip();
    void ShowTrayMenu();
    void ShowAbout() const;
};

}  // namespace typestatus
