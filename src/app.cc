#include "app.h"
#include "include/cef_v8.h"

OflApp::OflApp(const OflConfig& config) : config_(config) {}

void OflApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {

    // Native Wayland support
    command_line->AppendSwitchWithValue("ozone-platform", "wayland");
    command_line->AppendSwitch("enable-wayland-ime");

    // HiDPI + spellcheck (no VAAPI/WebRTC/PipeWire — not needed for email)
    command_line->AppendSwitchWithValue("enable-features",
        "UseOzonePlatform,WaylandWindowDecorations,SpellcheckServiceMultilingual");

    // GPU acceleration
    command_line->AppendSwitch("enable-gpu");
    command_line->AppendSwitch("enable-gpu-rasterization");

    // Custom user-agent — Outlook rejects non-Chrome browsers
    command_line->AppendSwitchWithValue("user-agent",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36");

    // Disable sandbox (no suid chrome-sandbox shipped)
    command_line->AppendSwitch("no-sandbox");

    // Autoplay for notification sounds
    command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");

    // Spellcheck
    command_line->AppendSwitch("enable-spelling-auto-correct");
}

void OflApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               CefRefPtr<CefV8Context> context) {
    std::string url = frame->GetURL().ToString();

    // Inject on all contexts — the hooks are harmless on non-Outlook pages,
    // and the URL may be empty/about:blank at this early stage

    // This runs BEFORE any page JavaScript — the V8 context just got created
    std::string js = R"JS(
(function() {
    if (window.__ofl_notif_hooked) return;
    window.__ofl_notif_hooked = true;

    // === 1. Hook Web Audio API (Outlook uses AudioBufferSource for notification sounds) ===
    var OrigAudioCtx = window.AudioContext || window.webkitAudioContext;
    if (OrigAudioCtx) {
        var origCreateBufferSource = OrigAudioCtx.prototype.createBufferSource;
        OrigAudioCtx.prototype.createBufferSource = function() {
            var src = origCreateBufferSource.apply(this, arguments);
            var origStart = src.start;
            src.start = function() {
                setTimeout(function() { __ofl_onNewMail(); }, 800);
                return origStart.apply(this, arguments);
            };
            return src;
        };
    }
    // Also hook HTMLAudioElement.play as fallback
    var OrigAudioPlay = HTMLAudioElement.prototype.play;
    HTMLAudioElement.prototype.play = function() {
        setTimeout(function() { __ofl_onNewMail(); }, 800);
        return OrigAudioPlay.apply(this, arguments);
    };

    // === 2. New mail handler: scrape unread item and fire notification ===
    window.__ofl_onNewMail = function() {
        try {
            var unread = document.querySelectorAll('[aria-label^="Unread"]');
            if (unread.length === 0) {
                console.log('__OFL_NOTIF__:' + JSON.stringify({
                    title: 'New mail', body: 'You have a new message', tag: 'ofl'
                }));
                return;
            }
            var item = unread[0];
            var label = item.getAttribute('aria-label') || '';
            // aria-label format: "Unread [flags] <sender> <subject> <time> <preview>"
            // Extract sender and subject by parsing the label
            var sender = '', subject = '';
            // Remove "Unread" prefix and known flags
            var rest = label.replace(/^Unread\s*/, '')
                           .replace(/^Collapsed\s*/, '')
                           .replace(/^Expanded\s*/, '')
                           .replace(/^Pinned\s*/, '')
                           .replace(/^Replied\s*/, '')
                           .replace(/^Forwarded\s*/, '')
                           .replace(/^Has attachment\s*/, '')
                           .replace(/^External\s*/, '')
                           .replace(/^sender\s*/, '');
            // Find time pattern (H:MM AM/PM or H:MM) to split sender+subject from preview
            var timeMatch = rest.match(/\s+\d{1,2}:\d{2}\s*(AM|PM)?\s/i);
            if (timeMatch) {
                var beforeTime = rest.substring(0, timeMatch.index).trim();
                // The text before time contains sender + subject
                // Try to find sender from child elements
                var senderEl = item.querySelector('[class*="sender"], [class*="from"], [class*="Sender"], [class*="From"]');
                var subjectEl = item.querySelector('[class*="subject"], [class*="Subject"]');
                if (senderEl) sender = senderEl.textContent.trim();
                if (subjectEl) subject = subjectEl.textContent.trim();
                // Fallback: use the text before time
                if (!sender && !subject) {
                    // Split on common patterns - first word(s) are sender, rest is subject
                    var parts = beforeTime.split(/\s{2,}/);
                    if (parts.length >= 2) {
                        sender = parts[0];
                        subject = parts.slice(1).join(' ');
                    } else {
                        sender = beforeTime;
                    }
                }
            }
            console.log('__OFL_NOTIF__:' + JSON.stringify({
                title: sender || 'New mail',
                body: subject || 'You have a new message',
                tag: 'ofl'
            }));
        } catch(e) {
            console.log('__OFL_NOTIF__:' + JSON.stringify({
                title: 'New mail', body: 'You have a new message', tag: 'ofl'
            }));
        }
    };

    // === 3. Badge count from Inbox folder title attribute ===
    var lastBadge = -1;
    function checkBadge() {
        try {
            var count = 0;
            // Inbox element has title="Inbox - N items (M unread)"
            var inbox = document.querySelector('[data-folder-name="inbox"], [title*="Inbox"]');
            if (inbox) {
                var title = inbox.getAttribute('title') || '';
                var match = title.match(/\((\d+)\s*unread\)/i);
                if (match) count = parseInt(match[1], 10);
            }
            // Fallback: count elements with aria-label starting with "Unread"
            if (!count) {
                var unread = document.querySelectorAll('[aria-label^="Unread"]');
                if (unread.length > 0) count = unread.length;
            }
            if (count !== lastBadge) {
                lastBadge = count;
                console.log('__OFL_BADGE__:' + count);
            }
        } catch(e) {}
    }
    setInterval(checkBadge, 3000);
    setTimeout(checkBadge, 5000);

    // === 4. Hook Notification API (fallback) ===
    var OrigNotification = window.Notification;
    if (OrigNotification) {
        function OflNotification(title, options) {
            options = options || {};
            console.log('__OFL_NOTIF__:' + JSON.stringify({
                title: title || '', body: options.body || '', tag: options.tag || ''
            }));
            try { return new OrigNotification(title, options); } catch(e) { return {}; }
        }
        OflNotification.prototype = OrigNotification.prototype;
        Object.defineProperty(OflNotification, 'permission', {
            get: function() { return 'granted'; }
        });
        OflNotification.requestPermission = function(cb) {
            if (cb) cb('granted');
            return Promise.resolve('granted');
        };
        window.Notification = OflNotification;
    }
})();
)JS";

    std::string inject_url = url.empty() ? "about:blank" : url;
    frame->ExecuteJavaScript(js, inject_url, 0);
}
