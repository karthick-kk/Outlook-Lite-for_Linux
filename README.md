# Outlook Lite for Linux

Lightweight, native Microsoft Outlook client for Linux using WebKitGTK and C++20.

No Electron. No bundled browser engine. Just a ~500KB binary that uses your system's WebKitGTK.

## Features

- System tray with unread badge count
- Desktop notifications for new emails
- Close-to-tray (configurable)
- Window state persistence (position + size)
- Custom CSS injection (`~/.config/ofl/custom.css`)
- OAuth popup handling for Microsoft sign-in
- Auto-download attachments to `~/Downloads`
- Keyboard shortcuts (F5 reload, F12 devtools, Ctrl+Q quit)
- Single-instance enforcement
- Spellcheck (built-in via WebKitGTK)
- Hardware-accelerated rendering

## Install

### Build from source

```bash
# Dependencies (Ubuntu/Debian)
sudo apt install build-essential cmake pkg-config \
  libwebkit2gtk-4.1-dev libgtk-3-dev \
  libayatana-appindicator3-dev libnotify-dev

# Dependencies (Fedora)
sudo dnf install gcc-c++ cmake pkg-config \
  webkit2gtk4.1-devel gtk3-devel \
  libayatana-appindicator-gtk3-devel libnotify-devel

# Dependencies (Arch)
sudo pacman -S cmake pkg-config webkit2gtk-4.1 gtk3 \
  libayatana-appindicator libnotify

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install (optional)
sudo make install
```

### Packages

Pre-built packages (.deb, .rpm, .pkg.tar.zst) are available on the [Releases](https://github.com/karthick-kk/outlook-lite-for-linux/releases) page.

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
```

### Environment variables

Environment variables override the config file:

| Variable | Description |
|---|---|
| `OFL_URL` | Override default URL |
| `OFL_WIDTH` | Override window width |
| `OFL_HEIGHT` | Override window height |
| `OFL_DEV_TOOLS` | Enable devtools (`true`/`1`) |

### Custom CSS

Place custom styles in `~/.config/ofl/custom.css` — they're injected into every page load.

## License

MIT
