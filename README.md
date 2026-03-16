# Outlook Lite for Linux

Lightweight, native Microsoft Outlook client for Linux using CEF (Chromium Embedded Framework) and C++20.

No Electron. No Node.js. A native C++ app with Chromium rendering, Wayland support, and persistent sessions.

## Features

- Chromium-based rendering via CEF (Chromium 133) — full Outlook web compatibility
- Native Wayland support with GPU acceleration
- Persistent sessions — login once, stay logged in
- System tray with unread badge count
- Desktop notifications for new emails
- Ad popup blocking (network-level)
- Frameless window mode (configurable)
- Close-to-tray (configurable)
- Window state persistence (position + size)
- Custom CSS injection (`~/.config/ofl/custom.css`)
- OAuth popup handling for Microsoft sign-in
- Auto-download attachments to `~/Downloads`
- Keyboard shortcuts (F5 reload, Ctrl+Shift+R bypass cache, F12 devtools, Ctrl+Q quit)
- Single-instance enforcement
- Spellcheck with context menu suggestions
- CI packaging: `.deb`, `.rpm`, `.pkg.tar.zst`, binary `tar.gz`

## Install

### Build from source

```bash
# Dependencies (Ubuntu/Debian)
sudo apt install build-essential cmake pkg-config curl patchelf \
  libgtk-3-dev libayatana-appindicator3-dev libnotify-dev libnss3-dev

# Dependencies (Fedora)
sudo dnf install gcc-c++ cmake pkg-config curl patchelf \
  gtk3-devel libayatana-appindicator-gtk3-devel libnotify-devel nss-devel

# Dependencies (Arch)
sudo pacman -S cmake pkg-config gtk3 libayatana-appindicator libnotify nss curl patchelf

# Download CEF (official minimal distribution, ~350MB)
bash packaging/download-cef.sh /tmp/cef-ofl

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCEF_ROOT=/tmp/cef-ofl
cmake --build build -j$(nproc)

# Run
./build/ofl
```

### Packages

Pre-built packages (`.deb`, `.rpm`, `.pkg.tar.zst`) are available on the [Releases](https://github.com/karthick-kk/Outlook-Lite-for_Linux/releases) page.

## Configuration

Config file: `~/.config/ofl/config`

```ini
# Default URL (outlook.office.com for work, outlook.live.com for personal)
url = https://outlook.office.com

# Window size
width = 1280
height = 800

# Enable browser developer tools (F12)
dev_tools = false

# Minimize to tray on close
close_to_tray = true

# Start minimized to tray
start_minimized = false

# Frameless window (no title bar)
frameless = true
```

### Data directories

| Path | Purpose |
|---|---|
| `~/.config/ofl/config` | Configuration file |
| `~/.config/ofl/custom.css` | Custom CSS (injected on page load) |
| `~/.local/share/ofl/` | Cookies, IndexedDB, session data (persistent) |
| `~/.cache/ofl/` | HTTP cache (disposable) |

### Environment variables

Environment variables override the config file:

| Variable | Description |
|---|---|
| `OFL_URL` | Override default URL |
| `OFL_WIDTH` | Override window width |
| `OFL_HEIGHT` | Override window height |
| `OFL_DEV_TOOLS` | Enable devtools (`true`/`1`) |

## Related

- [Teams Lite for Linux (TFL)](https://github.com/karthick-kk/teams-for-linux) — sister project for Microsoft Teams, using custom CEF with WebRTC/H.264/PipeWire support

## License

MIT
