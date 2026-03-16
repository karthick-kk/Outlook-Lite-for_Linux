#include "window.h"
#include "tray.h"
#include "notifications.h"
#include "config.h"
#include <cstdio>

// --- OflWindowDelegate ---

OflWindowDelegate::OflWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                                     const OflConfig& config)
    : browser_view_(browser_view), config_(config) {}

void OflWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
    window->AddChildView(browser_view_);
    window->Show();
    browser_view_->RequestFocus();

    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
        tray_init(browser, window);
    }

    CefRefPtr<CefWindow> win = window;
    notifications_set_click_handler([win]() {
        if (win) {
            win->Show();
            win->Activate();
        }
    });

    if (config_.start_minimized) {
        window->Hide();
    }

    fprintf(stderr, "[ofl] Window created\n");
}

void OflWindowDelegate::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
    tray_shutdown();
    browser_view_ = nullptr;
    fprintf(stderr, "[ofl] Window destroyed\n");
}

bool OflWindowDelegate::CanClose(CefRefPtr<CefWindow> window) {
    CefRect bounds = window->GetBounds();
    save_window_state(config_, bounds.x, bounds.y, bounds.width, bounds.height);

    if (tray_quit_requested()) {
        tray_shutdown();
        return true;
    }

    if (config_.close_to_tray) {
        window->Hide();
        return false;
    }

    tray_shutdown();
    return true;
}

CefRect OflWindowDelegate::GetInitialBounds(CefRefPtr<CefWindow> window) {
    if (config_.x >= 0 && config_.y >= 0) {
        return CefRect(config_.x, config_.y, config_.width, config_.height);
    }
    return CefRect(0, 0, config_.width, config_.height);
}

bool OflWindowDelegate::IsFrameless(CefRefPtr<CefWindow> window) {
    return config_.frameless;
}

bool OflWindowDelegate::GetLinuxWindowProperties(
    CefRefPtr<CefWindow> window,
    CefLinuxWindowProperties& properties) {
    CefString(&properties.wayland_app_id) = "ofl";
    CefString(&properties.wm_class_class) = "ofl";
    CefString(&properties.wm_class_name) = "ofl";
    return true;
}

// --- OflBrowserViewDelegate ---

OflBrowserViewDelegate::OflBrowserViewDelegate(const OflConfig& config)
    : config_(config) {}

bool OflBrowserViewDelegate::OnPopupBrowserViewCreated(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowserView> popup_browser_view,
    bool is_devtools) {
    CefWindow::CreateTopLevelWindow(
        new OflWindowDelegate(popup_browser_view, config_));
    return true;
}
