#pragma once

#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "config.h"

class OflWindowDelegate : public CefWindowDelegate {
public:
    explicit OflWindowDelegate(CefRefPtr<CefBrowserView> browser_view,
                               const OflConfig& config);

    void OnWindowCreated(CefRefPtr<CefWindow> window) override;
    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
    bool CanClose(CefRefPtr<CefWindow> window) override;
    CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override;
    bool IsFrameless(CefRefPtr<CefWindow> window) override;
    bool GetLinuxWindowProperties(CefRefPtr<CefWindow> window,
                                  CefLinuxWindowProperties& properties) override;

private:
    CefRefPtr<CefBrowserView> browser_view_;
    OflConfig config_;

    IMPLEMENT_REFCOUNTING(OflWindowDelegate);
    DISALLOW_COPY_AND_ASSIGN(OflWindowDelegate);
};

class OflBrowserViewDelegate : public CefBrowserViewDelegate {
public:
    explicit OflBrowserViewDelegate(const OflConfig& config);

    bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                   CefRefPtr<CefBrowserView> popup_browser_view,
                                   bool is_devtools) override;

private:
    OflConfig config_;

    IMPLEMENT_REFCOUNTING(OflBrowserViewDelegate);
    DISALLOW_COPY_AND_ASSIGN(OflBrowserViewDelegate);
};
