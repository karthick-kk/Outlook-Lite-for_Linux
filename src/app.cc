#include "app.h"

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
