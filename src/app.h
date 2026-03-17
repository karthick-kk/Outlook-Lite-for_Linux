#pragma once

#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"
#include "config.h"

class OflApp : public CefApp,
               public CefBrowserProcessHandler,
               public CefRenderProcessHandler {
public:
    explicit OflApp(const OflConfig& config);

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }

    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override;

    // CefRenderProcessHandler
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override;

private:
    OflConfig config_;
    IMPLEMENT_REFCOUNTING(OflApp);
    DISALLOW_COPY_AND_ASSIGN(OflApp);
};
