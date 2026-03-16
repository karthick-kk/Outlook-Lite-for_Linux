#include "app.h"
#include "client.h"
#include "window.h"
#include "config.h"
#include "notifications.h"
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include <cstdio>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);

    OflConfig config;
    CefRefPtr<OflApp> app(new OflApp(config));

    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    // --- Main browser process ---
    config = load_config();

    if (!acquire_instance_lock(config.config_dir)) {
        fprintf(stderr, "[ofl] Another instance is already running\n");
        return 0;
    }

    fprintf(stderr, "[ofl] Starting Outlook Lite for Linux (CEF)\n");

    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;
    settings.windowless_rendering_enabled = false;

    CefString(&settings.root_cache_path) = config.data_dir;
    CefString(&settings.cache_path) = config.data_dir + "/Default";
    CefString(&settings.locale) = "en-US";
    CefString(&settings.accept_language_list) = "en-US,en";
    CefString(&settings.log_file) = config.cache_dir + "/cef.log";
    settings.log_severity = LOGSEVERITY_WARNING;
    settings.remote_debugging_port = 9222;

    // User agent set via command-line flags in app.cc
    CefString(&settings.user_agent) =
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36";

    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    std::string exe_dir;
    if (len > 0) {
        exe_path[len] = '\0';
        CefString(&settings.browser_subprocess_path) = exe_path;
        exe_dir = fs::path(exe_path).parent_path().string();
    }

    if (!exe_dir.empty()) {
        CefString(&settings.resources_dir_path) = exe_dir;
        CefString(&settings.locales_dir_path) = exe_dir + "/locales";
    }

    if (!CefInitialize(main_args, settings, app, nullptr)) {
        fprintf(stderr, "[ofl] CefInitialize failed\n");
        return 1;
    }

    fprintf(stderr, "[ofl] CEF initialized, data: %s\n", config.data_dir.c_str());

    notifications_init();

    CefRefPtr<OflClient> client(new OflClient(config));

    CefBrowserSettings browser_settings;
    browser_settings.javascript_access_clipboard = STATE_ENABLED;
    browser_settings.local_storage = STATE_ENABLED;

    CefRefPtr<OflBrowserViewDelegate> bv_delegate(new OflBrowserViewDelegate(config));
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        client, config.url, browser_settings, nullptr, nullptr, bv_delegate);

    CefWindow::CreateTopLevelWindow(
        new OflWindowDelegate(browser_view, config));

    fprintf(stderr, "[ofl] Browser loading: %s\n", config.url.c_str());

    CefRunMessageLoop();

    browser_view = nullptr;
    client = nullptr;
    bv_delegate = nullptr;
    app = nullptr;

    notifications_shutdown();
    CefShutdown();
    fprintf(stderr, "[ofl] Shutdown complete\n");
    return 0;
}
