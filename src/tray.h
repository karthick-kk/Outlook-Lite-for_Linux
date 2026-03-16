#pragma once

#include "include/cef_browser.h"
#include "include/views/cef_window.h"
#include <string>

void tray_init(CefRefPtr<CefBrowser> browser, CefRefPtr<CefWindow> window);
void tray_set_tooltip(const std::string& text);
bool tray_quit_requested();
void tray_request_quit();
void tray_shutdown();
