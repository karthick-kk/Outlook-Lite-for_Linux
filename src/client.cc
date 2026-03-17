#include "client.h"
#include "tray.h"
#include "notifications.h"
#include "badge.h"
#include "include/cef_app.h"
#include "include/views/cef_browser_view.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

static bool is_outlook_domain(const std::string& url) {
    return url.find("outlook.office.com") != std::string::npos ||
           url.find("outlook.office365.com") != std::string::npos ||
           url.find("outlook.live.com") != std::string::npos ||
           url.find("login.microsoftonline.com") != std::string::npos ||
           url.find("login.live.com") != std::string::npos ||
           url.find("login.microsoft.com") != std::string::npos ||
           url.find("microsoft.com/devicelogin") != std::string::npos ||
           url.find("aadcdn.msftauth.net") != std::string::npos ||
           url.find("aadcdn.msauth.net") != std::string::npos;
}

// Ad/tracking domain blocklist — network-level blocking like uBlock Origin
static bool is_blocked_domain(const std::string& url) {
    static const char* blocked[] = {
        // Microsoft ad SDK
        "adsdkprod.azureedge.net",
        "adsdk.microsoft.com",
        "ads.microsoft.com",
        // General ad networks
        "doubleclick.net",
        "googlesyndication.com",
        "googleadservices.com",
        "google-analytics.com",
        "googletagmanager.com",
        "googletagservices.com",
        "adservice.google.com",
        "pagead2.googlesyndication.com",
        // Tracking
        "bat.bing.com",
        "c.bing.com",
        "c.msn.com",
        "a-ring-fallback.msedge.net",
        "fp.msedge.net",
        "ntp.msn.com",
        "assets.msn.com",
        // Outlook-specific ads
        "outlookads.live.com",
        "ads.prod.outlookads.live.com",
        "dsp.prod.outlookads.live.com",
        "native.prod.outlookads.live.com",
        // Telemetry (optional — keeps things snappy)
        "browser.events.data.msn.com",
        "self.events.data.microsoft.com",
        "mobile.events.data.microsoft.com",
        // Facebook tracking
        "connect.facebook.net",
        "pixel.facebook.com",
        // Other ad networks
        "amazon-adsystem.com",
        "criteo.com",
        "criteo.net",
        "taboola.com",
        "outbrain.com",
        "scorecardresearch.com",
        "quantserve.com",
        "adsymptotic.com",
        "adnxs.com",
        "rubiconproject.com",
        nullptr
    };

    for (int i = 0; blocked[i]; i++) {
        if (url.find(blocked[i]) != std::string::npos) {
            return true;
        }
    }
    return false;
}

OflClient::OflClient(const OflConfig& config) : config_(config) {}

// --- LifeSpan ---

void OflClient::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    browsers_.push_back(browser);
    fprintf(stderr, "[ofl] Browser created (total: %zu)\n", browsers_.size());
}

bool OflClient::DoClose(CefRefPtr<CefBrowser> browser) {
    if (browsers_.size() == 1) {
        is_closing_ = true;
    }
    return false;
}

void OflClient::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    for (auto it = browsers_.begin(); it != browsers_.end(); ++it) {
        if ((*it)->IsSame(browser)) {
            browsers_.erase(it);
            break;
        }
    }
    if (browsers_.empty()) {
        CefQuitMessageLoop();
    }
}

bool OflClient::OnBeforePopup(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int popup_id,
    const CefString& target_url,
    const CefString& target_frame_name,
    CefLifeSpanHandler::WindowOpenDisposition target_disposition,
    bool user_gesture,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue>& extra_info,
    bool* no_javascript_access) {

    std::string url = target_url.ToString();
    if (is_outlook_domain(url)) {
        return false;  // allow popup (OAuth)
    }

    // Block ad popups entirely
    if (is_blocked_domain(url)) {
        fprintf(stderr, "[ofl] Blocked ad popup: %s\n", url.c_str());
        return true;  // cancel
    }

    // Open external links in default browser
    std::string cmd = "xdg-open '" + url + "' &";
    system(cmd.c_str());
    return true;  // cancel popup
}

// --- Context Menu ---

void OflClient::OnBeforeContextMenu(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefContextMenuParams> params,
                                     CefRefPtr<CefMenuModel> model) {
    CefString misspelled = params->GetMisspelledWord();
    if (misspelled.length() > 0) {
        std::vector<CefString> suggestions;
        params->GetDictionarySuggestions(suggestions);

        model->Clear();

        if (suggestions.empty()) {
            model->AddItem(MENU_ID_NO_SPELLING_SUGGESTIONS, "No suggestions");
            model->SetEnabled(MENU_ID_NO_SPELLING_SUGGESTIONS, false);
        } else {
            for (size_t i = 0; i < suggestions.size() && i <= 4; i++) {
                model->AddItem(MENU_ID_SPELLCHECK_SUGGESTION_0 + (int)i, suggestions[i]);
            }
        }
        model->AddSeparator();
    }

    if (params->IsEditable()) {
        model->AddItem(MENU_ID_UNDO, "Undo");
        model->AddItem(MENU_ID_REDO, "Redo");
        model->AddSeparator();
        model->AddItem(MENU_ID_CUT, "Cut");
        model->AddItem(MENU_ID_COPY, "Copy");
        model->AddItem(MENU_ID_PASTE, "Paste");
        model->AddItem(MENU_ID_SELECT_ALL, "Select All");
    } else if (params->GetSelectionText().length() > 0) {
        model->AddItem(MENU_ID_COPY, "Copy");
    }
}

bool OflClient::OnContextMenuCommand(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefContextMenuParams> params,
                                      int command_id,
                                      CefContextMenuHandler::EventFlags event_flags) {
    if (command_id >= MENU_ID_SPELLCHECK_SUGGESTION_0 &&
        command_id <= MENU_ID_SPELLCHECK_SUGGESTION_LAST) {
        std::vector<CefString> suggestions;
        params->GetDictionarySuggestions(suggestions);
        int idx = command_id - MENU_ID_SPELLCHECK_SUGGESTION_0;
        if (idx >= 0 && idx < (int)suggestions.size()) {
            CefString replacement = suggestions[idx];
            frame->ExecuteJavaScript(
                "document.execCommand('insertText', false, '" +
                replacement.ToString() + "');",
                frame->GetURL(), 0);
        }
        return true;
    }
    return false;
}

// --- Downloads ---

bool OflClient::OnBeforeDownload(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefDownloadItem> download_item,
                                  const CefString& suggested_name,
                                  CefRefPtr<CefBeforeDownloadCallback> callback) {
    const char* home = std::getenv("HOME");
    std::string downloads_dir = std::string(home ? home : "/tmp") + "/Downloads";
    std::string path = downloads_dir + "/" + suggested_name.ToString();

    fprintf(stderr, "[ofl] Downloading: %s\n", path.c_str());
    callback->Continue(path, false);
    return true;
}

void OflClient::OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefDownloadItem> download_item,
                                   CefRefPtr<CefDownloadItemCallback> callback) {
    if (download_item->IsComplete()) {
        std::string filename = download_item->GetFullPath().ToString();
        auto slash = filename.rfind('/');
        std::string name = (slash != std::string::npos) ? filename.substr(slash + 1) : filename;
        notifications_show("Download complete", name);
        fprintf(stderr, "[ofl] Download complete: %s\n", filename.c_str());
    } else if (download_item->IsCanceled()) {
        fprintf(stderr, "[ofl] Download canceled\n");
    }
}

// --- Load ---

void OflClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int httpStatusCode) {
    if (!frame->IsMain()) return;

    std::string url = frame->GetURL().ToString();
    if (!is_outlook_domain(url)) return;

    // Inject custom CSS from ~/.config/ofl/custom.css
    std::string css_path = config_.config_dir + "/custom.css";
    std::ifstream css_file(css_path);
    if (css_file.is_open()) {
        std::ostringstream ss;
        ss << css_file.rdbuf();
        std::string css = ss.str();
        if (!css.empty()) {
            std::string escaped;
            for (char c : css) {
                if (c == '\\') escaped += "\\\\";
                else if (c == '\'') escaped += "\\'";
                else if (c == '\n') escaped += "\\n";
                else if (c == '\r') continue;
                else escaped += c;
            }
            std::string js = "(function(){var s=document.createElement('style');"
                             "s.textContent='" + escaped + "';"
                             "document.head.appendChild(s);})();";
            frame->ExecuteJavaScript(js, url, 0);
            fprintf(stderr, "[ofl] Custom CSS injected from %s\n", css_path.c_str());
        }
    }
}

// --- Display ---

void OflClient::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) {
    std::string t = title.ToString();

    int badge = 0;
    if (t.size() > 2 && t[0] == '(') {
        auto close = t.find(')');
        if (close != std::string::npos && close > 1) {
            std::string num = t.substr(1, close - 1);
            try { badge = std::stoi(num); } catch (...) {}
        }
    }

    std::string tooltip = "Outlook Lite for Linux";
    if (badge > 0) {
        tooltip += " (" + std::to_string(badge) + " unread)";
    }
    tray_set_tooltip(tooltip);
    badge_set_count(badge);

    if (badge > last_badge_ && badge > 0) {
        int new_msgs = badge - last_badge_;
        std::string body = std::to_string(new_msgs) + " new message" +
                           (new_msgs > 1 ? "s" : "");
        notifications_show("Microsoft Outlook", body);
    }
    last_badge_ = badge;

    auto views = CefBrowserView::GetForBrowser(browser);
    if (views) {
        auto window = views->GetWindow();
        if (window) {
            window->SetTitle(title);
        }
    }
}

// --- Console (Web Notification bridge) ---

bool OflClient::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                  cef_log_severity_t level,
                                  const CefString& message,
                                  const CefString& source,
                                  int line) {
    std::string msg = message.ToString();
    const std::string notif_prefix = "__OFL_NOTIF__:";
    const std::string badge_prefix = "__OFL_BADGE__:";

    if (msg.compare(0, notif_prefix.size(), notif_prefix) == 0) {
        std::string json = msg.substr(notif_prefix.size());
        // Simple JSON parsing for title and body
        std::string title, body;
        auto extract = [&](const std::string& key) -> std::string {
            std::string needle = "\"" + key + "\":\"";
            auto pos = json.find(needle);
            if (pos == std::string::npos) return "";
            pos += needle.size();
            auto end = json.find('"', pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        };
        title = extract("title");
        body = extract("body");
        if (!title.empty()) {
            notifications_show(title, body);
        }
        return true;  // suppress from console log
    }

    if (msg.compare(0, badge_prefix.size(), badge_prefix) == 0) {
        std::string count_str = msg.substr(badge_prefix.size());
        int badge = 0;
        try { badge = std::stoi(count_str); } catch (...) {}

        badge_set_count(badge);

        std::string tooltip = "Outlook Lite for Linux";
        if (badge > 0) {
            tooltip += " (" + std::to_string(badge) + " unread)";
        }
        tray_set_tooltip(tooltip);

        last_badge_ = badge;
        return true;
    }

    return false;
}

// --- Request ---

CefRefPtr<CefResourceRequestHandler> OflClient::GetResourceRequestHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool is_navigation,
    bool is_download,
    const CefString& request_initiator,
    bool& disable_default_handling) {
    return this;
}

cef_return_value_t OflClient::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback) {
    // No inline resource blocking — only popups are blocked (in OnBeforePopup)
    return RV_CONTINUE;
}

bool OflClient::OnCertificateError(
    CefRefPtr<CefBrowser> browser,
    cef_errorcode_t cert_error,
    const CefString& request_url,
    CefRefPtr<CefSSLInfo> ssl_info,
    CefRefPtr<CefCallback> callback) {
    fprintf(stderr, "[ofl] Certificate error for %s (code %d)\n",
            request_url.ToString().c_str(), cert_error);
    return false;
}

// --- Permission ---

bool OflClient::OnShowPermissionPrompt(
    CefRefPtr<CefBrowser> browser,
    uint64_t prompt_id,
    const CefString& requesting_origin,
    uint32_t requested_permissions,
    CefRefPtr<CefPermissionPromptCallback> callback) {
    std::string origin = requesting_origin.ToString();
    if (is_outlook_domain(origin)) {
        callback->Continue(CEF_PERMISSION_RESULT_ACCEPT);
        return true;
    }
    callback->Continue(CEF_PERMISSION_RESULT_DENY);
    return true;
}

// --- JS Dialogs ---

bool OflClient::OnJSDialog(CefRefPtr<CefBrowser> browser,
                            const CefString& origin_url,
                            CefJSDialogHandler::JSDialogType dialog_type,
                            const CefString& message_text,
                            const CefString& default_prompt_text,
                            CefRefPtr<CefJSDialogCallback> callback,
                            bool& suppress_message) {
    callback->Continue(true, CefString());
    return true;
}

bool OflClient::OnBeforeUnloadDialog(CefRefPtr<CefBrowser> browser,
                                      const CefString& message_text,
                                      bool is_reload,
                                      CefRefPtr<CefJSDialogCallback> callback) {
    callback->Continue(true, CefString());
    return true;
}

// --- Keyboard ---

bool OflClient::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                               const CefKeyEvent& event,
                               CefEventHandle os_event,
                               bool* is_keyboard_shortcut) {
    if (event.type != KEYEVENT_RAWKEYDOWN) return false;

    // F5 — reload
    if (event.windows_key_code == 0x74) {
        browser->Reload();
        return true;
    }

    // F12 — toggle devtools
    if (event.windows_key_code == 0x7B && config_.enable_dev_tools) {
        if (browser->GetHost()->HasDevTools()) {
            browser->GetHost()->CloseDevTools();
        } else {
            CefWindowInfo devtools_info;
            CefBrowserSettings devtools_settings;
            browser->GetHost()->ShowDevTools(devtools_info, nullptr, devtools_settings, CefPoint());
        }
        return true;
    }

    // Ctrl+R — reload
    if (event.windows_key_code == 'R' && (event.modifiers & EVENTFLAG_CONTROL_DOWN)) {
        if (event.modifiers & EVENTFLAG_SHIFT_DOWN) {
            browser->ReloadIgnoreCache();
        } else {
            browser->Reload();
        }
        return true;
    }

    // Ctrl+Q — quit
    if (event.windows_key_code == 'Q' && (event.modifiers & EVENTFLAG_CONTROL_DOWN)) {
        tray_request_quit();
        return true;
    }

    // Alt+F4 — quit
    if (event.windows_key_code == 0x73 && (event.modifiers & EVENTFLAG_ALT_DOWN)) {
        tray_request_quit();
        return true;
    }

    return false;
}
