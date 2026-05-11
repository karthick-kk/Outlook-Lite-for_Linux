// OFL — Outlook Lite for Linux: JS injection script
// Hooks audio playback, scrapes unread mail, polls badge count.
(function () {
  "use strict";

  // Guard against double-injection
  if (window.__ofl_injected) return;
  window.__ofl_injected = true;

  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  function invoke(cmd, args) {
    try {
      if (window.__TAURI__ && window.__TAURI__.core) {
        window.__TAURI__.core.invoke(cmd, args);
      }
    } catch (e) {
      console.warn("[OFL] invoke failed:", cmd, e);
    }
  }

  // ---------------------------------------------------------------------------
  // 1. AudioContext.createBufferSource hook
  //    Outlook uses Web Audio API for notification sounds.
  // ---------------------------------------------------------------------------

  const origCreateBufferSource =
    AudioContext.prototype.createBufferSource;

  AudioContext.prototype.createBufferSource = function () {
    const source = origCreateBufferSource.apply(this, arguments);
    const origStart = source.start.bind(source);

    source.start = function () {
      // Notification sound detected — scrape mail
      setTimeout(__ofl_onNewMail, 500);
      return origStart.apply(this, arguments);
    };

    return source;
  };

  // ---------------------------------------------------------------------------
  // 2. HTMLAudioElement.play hook (fallback)
  // ---------------------------------------------------------------------------

  const origPlay = HTMLAudioElement.prototype.play;

  HTMLAudioElement.prototype.play = function () {
    setTimeout(__ofl_onNewMail, 500);
    return origPlay.apply(this, arguments);
  };

  // ---------------------------------------------------------------------------
  // 3. Mail scraping — parse unread items from aria-label
  // ---------------------------------------------------------------------------

  const FLAGS_RE =
    /^(Collapsed|Expanded|Pinned|Replied|Forwarded|Has attachment|External|)\s*/;

  function parseUnreadLabel(label) {
    // Format: "Unread [flags...] <sender> <subject> <time> <preview>"
    // Strip leading "Unread" marker
    let text = label.replace(/^Unread\s*/, "");

    // Strip known flags (they appear as space-separated tokens before sender)
    let changed = true;
    while (changed) {
      changed = false;
      const m = text.match(FLAGS_RE);
      if (m && m[0].length > 0) {
        text = text.slice(m[0].length);
        changed = true;
      }
    }

    // After flags, the next token is the sender (up to first space-separated segment
    // that looks like a subject). Heuristic: sender is the first "word" or quoted name.
    // Outlook typically uses the display name as a single token before the subject line.
    // We split on the first two whitespace-delimited segments.
    const parts = text.split(/\s+/);
    const sender = parts[0] || "Unknown";
    const subject = parts.slice(1, 8).join(" ") || "(no subject)";

    return { sender, subject };
  }

  window.__ofl_onNewMail = function __ofl_onNewMail() {
    try {
      const items = document.querySelectorAll('[aria-label^="Unread"]');
      if (items.length === 0) return;

      // Only report the first (newest) unread item
      const label = items[0].getAttribute("aria-label") || "";
      const { sender, subject } = parseUnreadLabel(label);

      invoke("new_mail", { sender, subject });
    } catch (e) {
      console.warn("[OFL] mail scrape error:", e);
    }
  };

  // ---------------------------------------------------------------------------
  // 4. Badge polling — check inbox unread count every 3 seconds
  // ---------------------------------------------------------------------------

  let lastBadgeCount = -1;

  function pollBadge() {
    try {
      const inbox = document.querySelector('[data-folder-name="inbox"]');
      if (!inbox) return;

      const title = inbox.getAttribute("title") || "";
      const m = title.match(/\((\d+)\s+unread\)/i);
      const count = m ? parseInt(m[1], 10) : 0;

      if (count !== lastBadgeCount) {
        lastBadgeCount = count;
        invoke("update_badge", { count });
      }
    } catch (e) {
      console.warn("[OFL] badge poll error:", e);
    }
  }

  setInterval(pollBadge, 3000);
  // Run once immediately after DOM is likely ready
  setTimeout(pollBadge, 2000);

  // ---------------------------------------------------------------------------
  // 5. Notification API shim (fallback)
  //    Intercept browser Notification to route through native notifications.
  // ---------------------------------------------------------------------------

  const OrigNotification = window.Notification;

  window.Notification = function (title, options) {
    const body = (options && options.body) || "";
    invoke("new_mail", { sender: title, subject: body });
    // Still create the original notification for Outlook's internal tracking
    try {
      return new OrigNotification(title, options);
    } catch (e) {
      return {};
    }
  };

  // Preserve static properties
  window.Notification.permission = "granted";
  window.Notification.requestPermission = function (cb) {
    if (cb) cb("granted");
    return Promise.resolve("granted");
  };

  // ---------------------------------------------------------------------------
  // 6. Keyboard shortcuts
  // ---------------------------------------------------------------------------

  document.addEventListener('keydown', function(e) {
    if (e.ctrlKey && e.shiftKey && e.key === 'R') {
      e.preventDefault();
      invoke('reload_no_cache');
    } else if (e.key === 'F5' || (e.ctrlKey && e.key === 'r')) {
      e.preventDefault();
      invoke('reload');
    } else if (e.key === 'F12') {
      e.preventDefault();
      invoke('toggle_devtools');
    } else if (e.ctrlKey && e.key === 'q') {
      e.preventDefault();
      invoke('quit');
    }
  });

  console.log("[OFL] inject.js loaded");
})();
