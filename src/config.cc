#include "config.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>
#include <sys/file.h>
#include <unistd.h>

namespace fs = std::filesystem;

static int lock_fd = -1;

static std::string get_xdg_dir(const char* env_var, const char* fallback_suffix) {
    const char* val = std::getenv(env_var);
    if (val && val[0]) return std::string(val) + "/ofl";
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::string(home) + fallback_suffix;
}

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 &&
        ((s.front() == '"' && s.back() == '"') ||
         (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static void parse_config_file(const std::string& path, OflConfig& config) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = unquote(trim(line.substr(eq + 1)));

        if (key == "url") config.url = val;
        else if (key == "width") { try { config.width = std::stoi(val); } catch (...) {} }
        else if (key == "height") { try { config.height = std::stoi(val); } catch (...) {} }
        else if (key == "x") { try { config.x = std::stoi(val); } catch (...) {} }
        else if (key == "y") { try { config.y = std::stoi(val); } catch (...) {} }
        else if (key == "dev_tools") config.enable_dev_tools = (val == "true" || val == "1");
        else if (key == "start_minimized") config.start_minimized = (val == "true" || val == "1");
        else if (key == "close_to_tray") config.close_to_tray = (val == "true" || val == "1");
        else if (key == "frameless") config.frameless = (val == "true" || val == "1");
    }
}

static void write_default_config(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "# Outlook Lite for Linux configuration\n"
         << "#\n"
         << "# url = https://outlook.office.com\n"
         << "# width = 1280\n"
         << "# height = 800\n"
         << "# dev_tools = false\n"
         << "# start_minimized = false\n"
         << "# close_to_tray = true\n"
         << "# frameless = true\n";
}

OflConfig load_config() {
    OflConfig config;
    config.config_dir = get_xdg_dir("XDG_CONFIG_HOME", "/.config/ofl");
    config.cache_dir = get_xdg_dir("XDG_CACHE_HOME", "/.cache/ofl");
    config.data_dir = get_xdg_dir("XDG_DATA_HOME", "/.local/share/ofl");

    fs::create_directories(config.config_dir);
    fs::create_directories(config.cache_dir);
    fs::create_directories(config.data_dir);

    // Load config file
    std::string config_path = config.config_dir + "/config";
    if (!fs::exists(config_path)) {
        write_default_config(config_path);
    }
    parse_config_file(config_path, config);

    // Load saved window state (overrides config file geometry)
    std::string state_path = config.config_dir + "/window-state";
    parse_config_file(state_path, config);

    // Environment variables override everything
    const char* env;
    if ((env = std::getenv("OFL_URL")) && env[0]) config.url = env;
    if ((env = std::getenv("OFL_WIDTH")) && env[0]) { try { config.width = std::stoi(env); } catch (...) {} }
    if ((env = std::getenv("OFL_HEIGHT")) && env[0]) { try { config.height = std::stoi(env); } catch (...) {} }
    if ((env = std::getenv("OFL_DEV_TOOLS")) && env[0]) config.enable_dev_tools = (std::string(env) == "true" || std::string(env) == "1");

    return config;
}

void save_window_state(const OflConfig& config, int x, int y, int w, int h) {
    std::string path = config.config_dir + "/window-state";
    std::ofstream file(path);
    if (!file.is_open()) return;
    file << "x = " << x << "\n"
         << "y = " << y << "\n"
         << "width = " << w << "\n"
         << "height = " << h << "\n";
}

bool acquire_instance_lock(const std::string& config_dir) {
    std::string lock_path = config_dir + "/ofl.lock";
    lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0600);
    if (lock_fd < 0) return false;
    if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        close(lock_fd);
        lock_fd = -1;
        return false;
    }
    return true;
}
