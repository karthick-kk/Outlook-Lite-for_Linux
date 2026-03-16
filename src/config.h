#pragma once

#include <string>

struct OflConfig {
    std::string url = "https://outlook.office.com";
    std::string config_dir;   // ~/.config/ofl
    std::string cache_dir;    // ~/.cache/ofl
    std::string data_dir;     // ~/.local/share/ofl
    int width = 1280;
    int height = 800;
    int x = -1;              // -1 = auto-center
    int y = -1;
    bool enable_dev_tools = false;
    bool start_minimized = false;
    bool close_to_tray = true;
    bool frameless = true;
};

// Load config from file + env vars, create dirs if needed
OflConfig load_config();

// Save window geometry for next launch
void save_window_state(const OflConfig& config, int x, int y, int w, int h);

// Single-instance file lock — returns true if lock acquired
bool acquire_instance_lock(const std::string& config_dir);
