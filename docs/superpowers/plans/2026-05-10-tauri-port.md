# Tauri Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Outlook Lite for Linux from CEF/C++ to Tauri v2/Rust for better memory usage and performance, while fixing session persistence.

**Architecture:** Tauri v2 app with Rust backend handling native integrations (notifications, badge, config) and JS init scripts for mail detection. WebKitGTK provides the web engine as a system library. All auth flows stay in-webview for session persistence.

**Tech Stack:** Rust, Tauri v2, WebKitGTK, notify-rust, zbus, serde

---

## File Structure

Files to be created:

| File | Purpose |
|------|---------|
| `src-tauri/Cargo.toml` | Rust dependencies |
| `src-tauri/tauri.conf.json` | Tauri configuration |
| `src-tauri/build.rs` | Tauri build script |
| `src-tauri/src/main.rs` | App bootstrap, single-instance |
| `src-tauri/src/lib.rs` | Tauri commands (IPC) |
| `src-tauri/src/config.rs` | INI config parser + XDG paths |
| `src-tauri/src/notifications.rs` | notify-rust notifications |
| `src-tauri/src/badge.rs` | D-Bus Unity LauncherEntry via zbus |
| `src-tauri/src/blocker.rs` | Ad/tracker domain blocklist |
| `src/inject.js` | JS injection script (audio hook, DOM scraping, badge polling) |
| `data/ofl.desktop` | Desktop entry (keep existing) |
| `data/ofl-app.svg` | App icon (keep existing) |
| `data/ofl.svg` | Tray icon (keep existing) |
| `packaging/build-local.sh` | Updated for Tauri builds |

---

## Task 1: Scaffold Tauri v2 Project

Remove all C++ source files and CEF build system. Initialize Tauri v2 project structure.

- [ ] Remove C++ source files (src/*.cc, src/*.h, CMakeLists.txt, packaging/download-cef.sh)
- [ ] Install Tauri CLI: `cargo install tauri-cli`
- [ ] Create `src-tauri/Cargo.toml` with dependencies: tauri 2 (features: devtools), serde (features: derive), serde_json, notify-rust 4, zbus 4, once_cell, dirs 5, nix (features: fs) for flock
- [ ] Create `src-tauri/build.rs` with `tauri_build::build()`
- [ ] Create `src-tauri/tauri.conf.json` — app identifier "dev.ofl.outlook-lite", window: title "Outlook Lite for Linux", decorations false, width 1280, height 800, url https://outlook.office.com, transparent false. Security: dangerousRemoteUrlAccess patterns for outlook.office.com, outlook.cloud.microsoft, login.microsoftonline.com, login.live.com, login.microsoft.com. withGlobalTauri: true.
- [ ] Create minimal `src-tauri/src/main.rs` that just starts Tauri with a default window
- [ ] Create `src-tauri/src/lib.rs` with empty `run()` function
- [ ] Run `cargo build` in src-tauri to verify it compiles
- [ ] Commit: "feat: scaffold Tauri v2 project structure"

---

## Task 2: Config Module

Implement the INI config parser matching the existing format.

- [ ] Create `src-tauri/src/config.rs` with:
  - `OflConfig` struct: url (String), width (u32), height (u32), x (i32), y (i32), dev_tools (bool), start_minimized (bool), close_to_background (bool), frameless (bool)
  - `load_config()` function: reads `~/.config/ofl/config`, parses key=value lines, handles # comments, quoted values
  - `save_window_state()`: saves x, y, width, height to `~/.config/ofl/window-state`
  - `load_window_state()`: reads window-state file
  - `default_config()`: creates default config file if missing
  - Environment variable overrides: OFL_URL, OFL_WIDTH, OFL_HEIGHT, OFL_DEV_TOOLS
  - XDG paths: config_dir, data_dir, cache_dir
- [ ] Add `mod config;` to lib.rs
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add config module with INI parser"

---

## Task 3: Single Instance Lock

Implement flock-based single instance enforcement.

- [ ] In `main.rs`, add single-instance check using `nix::fcntl::flock()` on `~/.local/share/ofl/ofl.lock`
- [ ] If lock fails, attempt to raise existing window (print message and exit for now)
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add single-instance lock"

---

## Task 4: Window Management

Configure Tauri window with proper settings from config.

- [ ] Update `src-tauri/src/lib.rs`:
  - Load config on startup
  - Apply window state (position, size) from saved state
  - Set up `on_close_requested` handler: hide window if close_to_background, else quit
  - Set up window `moved` and `resized` events to save state
- [ ] Update `tauri.conf.json` to not create a default window (we create it programmatically)
- [ ] In `lib.rs` `run()` function, create window with WebviewUrl::External pointing to config.url
- [ ] Set user-agent to Chrome 133 UA string via webview builder
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add window management with state persistence"

---

## Task 5: Navigation & External Links

Handle navigation events — keep auth in-webview, open external links in browser.

- [ ] In `lib.rs`, add navigation handler:
  - Allow navigation to: outlook.office.com, outlook.cloud.microsoft, outlook.office365.com, outlook.live.com, login.microsoftonline.com, login.live.com, login.microsoft.com, aadcdn.msftauth.net, aadcdn.msauth.net
  - For other URLs: open with `open::that(url)` and cancel navigation
- [ ] Add `open = "5"` to Cargo.toml dependencies
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add navigation handling with external link support"

---

## Task 6: Ad/Tracker Blocking

Implement domain blocklist for sub-resource requests.

- [ ] Create `src-tauri/src/blocker.rs`:
  - `static BLOCKED: Lazy<HashSet<&str>>` with all 30+ domains from current C++ code
  - `pub fn is_blocked(url: &str) -> bool` — checks if URL contains any blocked domain
- [ ] Add `mod blocker;` to lib.rs
- [ ] Register `on_web_resource_request` handler in webview builder that returns empty response for blocked URLs
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add ad/tracker blocking"

---

## Task 7: JS Injection Script

Create the inject.js that hooks audio and scrapes mail.

- [ ] Create `src/inject.js` with:
  - AudioContext.createBufferSource hook (detect notification sounds)
  - HTMLAudioElement.play hook (fallback)
  - `__ofl_onNewMail()` function: scrape `[aria-label^="Unread"]` elements, parse sender/subject from aria-label, call `window.__TAURI__.core.invoke('new_mail', {sender, subject})`
  - Badge polling: every 3s check `[data-folder-name="inbox"]` title attribute for `(N unread)`, call `window.__TAURI__.core.invoke('update_badge', {count})`
  - Notification API shim (fallback)
- [ ] Register inject.js as initialization script in Tauri webview config
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add JS injection for mail detection"

---

## Task 8: Notifications Module

Implement native desktop notifications via notify-rust.

- [ ] Create `src-tauri/src/notifications.rs`:
  - `pub fn show(sender: &str, subject: &str)` — creates and shows notification with icon "ofl-app", timeout 5000ms
  - `pub fn init()` — no-op for now (notify-rust doesn't need init)
- [ ] Add `mod notifications;` to lib.rs
- [ ] Add Tauri command `new_mail` in lib.rs that calls `notifications::show()`
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add desktop notifications via notify-rust"

---

## Task 9: Badge Module

Implement D-Bus Unity LauncherEntry badge via zbus.

- [ ] Create `src-tauri/src/badge.rs`:
  - `pub struct BadgeService` with zbus connection
  - `pub async fn init() -> BadgeService` — connect to session bus
  - `pub async fn set_count(&self, count: u32)` — emit Update signal on com.canonical.Unity.LauncherEntry with count and count-visible properties
  - Desktop file URI: "application://ofl.desktop"
- [ ] Add `mod badge;` to lib.rs
- [ ] Add Tauri command `update_badge` in lib.rs that calls badge service
- [ ] Use `tauri::async_runtime` for the async zbus calls
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add dock badge via D-Bus Unity LauncherEntry"

---

## Task 10: Keyboard Shortcuts

Add keyboard shortcut handling.

- [ ] In lib.rs, register global shortcuts or use Tauri's menu accelerators:
  - F5 / Ctrl+R: reload webview
  - Ctrl+Shift+R: reload ignoring cache
  - F12: toggle devtools (if config.dev_tools enabled)
  - Ctrl+Q: quit app
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add keyboard shortcuts"

---

## Task 11: Custom CSS Injection

Add support for user custom CSS from config dir.

- [ ] In lib.rs, on webview creation:
  - Read `~/.config/ofl/custom.css` if it exists
  - Add as initialization script that creates a style element and appends to head
- [ ] Run `cargo build` to verify
- [ ] Commit: "feat: add custom CSS injection"

---

## Task 12: Packaging & CI

Update packaging for Tauri builds.

- [ ] Update `.github/workflows/build.yml`:
  - Replace CMake/CEF build steps with Rust/Tauri build
  - Install system deps: libwebkit2gtk-4.1-dev, libgtk-3-dev, libayatana-appindicator3-dev, librsvg2-dev, libssl-dev
  - Build with `cargo tauri build --bundles deb,rpm`
  - For Arch: use cargo build + manual PKGBUILD
  - For binary tarball: cargo build --release + tar
- [ ] Update `packaging/PKGBUILD` for Rust/Tauri build
- [ ] Update `packaging/build-local.sh` for Tauri
- [ ] Keep `data/ofl.desktop` and icons unchanged
- [ ] Commit: "feat: update packaging for Tauri builds"

---

## Task 13: Integration Test

Manual verification of all features.

- [ ] Build with `cargo tauri build --bundles deb`
- [ ] Install the .deb
- [ ] Verify: Outlook loads correctly
- [ ] Verify: Session persists after restart (no re-login)
- [ ] Verify: Notification fires on new mail
- [ ] Verify: Badge count updates
- [ ] Verify: Ads are blocked
- [ ] Verify: External links open in browser
- [ ] Verify: Custom CSS applies
- [ ] Verify: Keyboard shortcuts work
- [ ] Verify: Window state persists
- [ ] Verify: Close-to-background works, re-launch raises window
- [ ] Measure: RAM usage at idle (target < 200MB)
- [ ] Measure: Startup time (target < 1.5s)
- [ ] Commit any fixes needed
