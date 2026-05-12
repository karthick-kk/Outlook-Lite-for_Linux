// OFL — Outlook Lite for Linux: JS injection script
// State is stored in window.__ofl_state for Rust-side polling via eval.
(function () {
  "use strict";

  if (window.__ofl_injected) return;
  window.__ofl_injected = true;

  // Shared state — Rust reads this via eval
  window.__ofl_state = {
    notifications: [],
    badgeCount: -1
  };

  // ---------------------------------------------------------------------------
  // 1. AudioContext.createBufferSource hook
  // ---------------------------------------------------------------------------

  var OrigAudioCtx = window.AudioContext || window.webkitAudioContext;
  if (OrigAudioCtx) {
    var origCreateBufferSource = OrigAudioCtx.prototype.createBufferSource;
    OrigAudioCtx.prototype.createBufferSource = function () {
      var src = origCreateBufferSource.apply(this, arguments);
      var origStart = src.start;
      src.start = function () {
        setTimeout(onNewMail, 800);
        return origStart.apply(this, arguments);
      };
      return src;
    };
  }

  // ---------------------------------------------------------------------------
  // 2. HTMLAudioElement.play hook (fallback)
  // ---------------------------------------------------------------------------

  var origPlay = HTMLAudioElement.prototype.play;
  HTMLAudioElement.prototype.play = function () {
    setTimeout(onNewMail, 800);
    return origPlay.apply(this, arguments);
  };

  // ---------------------------------------------------------------------------
  // 3. New mail handler — push to state queue
  // ---------------------------------------------------------------------------

  function onNewMail() {
    try {
      var unread = document.querySelectorAll('[aria-label^="Unread"]');
      var sender = "New mail";
      var subject = "You have a new message";

      if (unread.length > 0) {
        var item = unread[0];
        var label = item.getAttribute("aria-label") || "";

        var rest = label
          .replace(/^Unread\s*/, "")
          .replace(/^Collapsed\s*/, "")
          .replace(/^Expanded\s*/, "")
          .replace(/^Pinned\s*/, "")
          .replace(/^Replied\s*/, "")
          .replace(/^Forwarded\s*/, "")
          .replace(/^Has attachment\s*/, "")
          .replace(/^External\s*/, "")
          .replace(/^sender\s*/, "");

        var timeMatch = rest.match(/\s+\d{1,2}:\d{2}\s*(AM|PM)?\s/i);
        if (timeMatch) {
          var beforeTime = rest.substring(0, timeMatch.index).trim();
          var parts = beforeTime.split(/\s{2,}/);
          if (parts.length >= 2) {
            sender = parts[0];
            subject = parts.slice(1).join(" ");
          } else {
            sender = beforeTime;
          }
        } else {
          sender = rest.substring(0, 50);
        }
      }

      window.__ofl_state.notifications.push({ sender: sender, subject: subject });
    } catch (e) {
      window.__ofl_state.notifications.push({ sender: "New mail", subject: "You have a new message" });
    }
  }

  // ---------------------------------------------------------------------------
  // 4. Badge count polling — updates state
  // ---------------------------------------------------------------------------

  function checkBadge() {
    try {
      var count = 0;
      var inbox = document.querySelector(
        '[data-folder-name="inbox"], [title*="Inbox"]'
      );
      if (inbox) {
        var title = inbox.getAttribute("title") || "";
        var match = title.match(/\((\d+)\s*unread\)/i);
        if (match) count = parseInt(match[1], 10);
      }
      if (!count) {
        var unread = document.querySelectorAll('[aria-label^="Unread"]');
        if (unread.length > 0) count = unread.length;
      }
      window.__ofl_state.badgeCount = count;
    } catch (e) {}
  }
  setInterval(checkBadge, 3000);
  setTimeout(checkBadge, 5000);

  // ---------------------------------------------------------------------------
  // 5. Notification API shim
  // ---------------------------------------------------------------------------

  var OrigNotification = window.Notification;
  if (OrigNotification) {
    function OflNotification(title, options) {
      options = options || {};
      window.__ofl_state.notifications.push({
        sender: title || "New mail",
        subject: options.body || "You have a new message"
      });
      try { return new OrigNotification(title, options); } catch (e) { return {}; }
    }
    OflNotification.prototype = OrigNotification.prototype;
    Object.defineProperty(OflNotification, "permission", {
      get: function () { return "granted"; },
    });
    OflNotification.requestPermission = function (cb) {
      if (cb) cb("granted");
      return Promise.resolve("granted");
    };
    window.Notification = OflNotification;
  }

  // ---------------------------------------------------------------------------
  // 6. Keyboard shortcuts
  // ---------------------------------------------------------------------------

  document.addEventListener("keydown", function (e) {
    if (e.key === "F5" || (e.ctrlKey && !e.shiftKey && e.key === "r")) {
      e.preventDefault();
      window.location.reload();
    } else if (e.ctrlKey && e.shiftKey && e.key === "R") {
      e.preventDefault();
      window.location.reload();
    } else if (e.ctrlKey && e.key === "q") {
      e.preventDefault();
      window.__ofl_state.quit = true;
    }
  });

  // ---------------------------------------------------------------------------
  // 7. External link handling (target="_blank" and regular external links)
  // ---------------------------------------------------------------------------

  // Store URLs to open externally — Rust polls this
  window.__ofl_state.pendingUrls = [];

  document.addEventListener("click", function (e) {
    var link = e.target.closest("a[href]");
    if (!link) return;
    var href = link.getAttribute("href");
    if (!href || href.startsWith("#") || href.startsWith("javascript:")) return;

    try {
      var url = new URL(href, window.location.origin);
      var host = url.hostname;
      var allowed = [
        "outlook.office.com",
        "outlook.cloud.microsoft",
        "outlook.office365.com",
        "outlook.live.com",
        "login.microsoftonline.com",
        "login.live.com",
        "login.microsoft.com",
        "aadcdn.msftauth.net",
        "aadcdn.msauth.net"
      ];
      var isAllowed = allowed.some(function(d) {
        return host === d || host.endsWith("." + d);
      });
      if (!isAllowed) {
        e.preventDefault();
        e.stopPropagation();
        window.__ofl_state.pendingUrls.push(url.href);
      }
    } catch(ex) {}
  }, true);

  // Also override window.open for target="_blank" links
  var origOpen = window.open;
  window.open = function(url) {
    if (!url) return origOpen.apply(this, arguments);
    try {
      var parsed = new URL(url, window.location.origin);
      var host = parsed.hostname;
      var allowed = [
        "outlook.office.com",
        "outlook.cloud.microsoft",
        "outlook.office365.com",
        "outlook.live.com",
        "login.microsoftonline.com",
        "login.live.com",
        "login.microsoft.com",
        "aadcdn.msftauth.net",
        "aadcdn.msauth.net"
      ];
      var isAllowed = allowed.some(function(d) {
        return host === d || host.endsWith("." + d);
      });
      if (!isAllowed) {
        window.__ofl_state.pendingUrls.push(parsed.href);
        return null;
      }
    } catch(ex) {}
    return origOpen.apply(this, arguments);
  };
})();
