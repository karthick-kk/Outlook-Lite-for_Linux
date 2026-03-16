#pragma once

#include "include/cef_app.h"
#include "config.h"

class OflApp : public CefApp, public CefBrowserProcessHandler {
public:
    explicit OflApp(const OflConfig& config);

    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
    void OnBeforeCommandLineProcessing(const CefString& process_type,
                                       CefRefPtr<CefCommandLine> command_line) override;

private:
    OflConfig config_;
    IMPLEMENT_REFCOUNTING(OflApp);
    DISALLOW_COPY_AND_ASSIGN(OflApp);
};
