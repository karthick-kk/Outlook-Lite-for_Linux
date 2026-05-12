use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

/// Application configuration parsed from ~/.config/ofl/config
#[derive(Debug, Clone)]
pub struct OflConfig {
    pub url: String,
    pub width: u32,
    pub height: u32,
    pub x: i32,
    pub y: i32,
    pub dev_tools: bool,
    pub start_minimized: bool,
    pub close_to_background: bool,
    pub frameless: bool,
}

/// Window position/size state persisted between sessions
#[derive(Debug, Clone)]
pub struct WindowState {
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
}

impl Default for OflConfig {
    fn default() -> Self {
        Self {
            url: "https://outlook.office.com".to_string(),
            width: 1280,
            height: 800,
            x: -1,
            y: -1,
            dev_tools: false,
            start_minimized: false,
            close_to_background: true,
            frameless: true,
        }
    }
}

/// Returns the XDG config directory for ofl (~/.config/ofl)
pub fn config_dir() -> PathBuf {
    dirs::config_dir()
        .unwrap_or_else(|| PathBuf::from("~/.config"))
        .join("ofl")
}

/// Returns the XDG data directory for ofl (~/.local/share/ofl)
pub fn data_dir() -> PathBuf {
    dirs::data_dir()
        .unwrap_or_else(|| PathBuf::from("~/.local/share"))
        .join("ofl")
}

/// Returns the XDG cache directory for ofl (~/.cache/ofl)
pub fn cache_dir() -> PathBuf {
    dirs::cache_dir()
        .unwrap_or_else(|| PathBuf::from("~/.cache"))
        .join("ofl")
}

/// Parses an INI-style config file into a key-value map.
/// Handles # and ; comments, key = value format, and quoted values.
fn parse_ini(content: &str) -> HashMap<String, String> {
    let mut map = HashMap::new();
    for line in content.lines() {
        let trimmed = line.trim();
        // Skip empty lines and comments
        if trimmed.is_empty() || trimmed.starts_with('#') || trimmed.starts_with(';') {
            continue;
        }
        // Split on first '='
        if let Some((key, value)) = trimmed.split_once('=') {
            let key = key.trim().to_string();
            let mut value = value.trim().to_string();
            // Strip optional quotes
            if (value.starts_with('"') && value.ends_with('"'))
                || (value.starts_with('\'') && value.ends_with('\''))
            {
                if value.len() >= 2 {
                    value = value[1..value.len() - 1].to_string();
                }
            }
            map.insert(key, value);
        }
    }
    map
}

/// Parses a string as a boolean value.
/// Accepts: true/false, yes/no, 1/0
fn parse_bool(s: &str) -> Option<bool> {
    match s.to_lowercase().as_str() {
        "true" | "yes" | "1" => Some(true),
        "false" | "no" | "0" => Some(false),
        _ => None,
    }
}

/// Creates the default config file if it doesn't exist.
pub fn default_config() -> std::io::Result<PathBuf> {
    let dir = config_dir();
    fs::create_dir_all(&dir)?;
    let path = dir.join("config");
    if !path.exists() {
        let content = "\
# Outlook Lite for Linux configuration
url = https://outlook.office.com
width = 1280
height = 800
dev_tools = false
start_minimized = false
close_to_tray = true
frameless = false
";
        fs::write(&path, content)?;
    }
    Ok(path)
}

/// Loads configuration from ~/.config/ofl/config with environment variable overrides.
pub fn load_config() -> OflConfig {
    let mut config = OflConfig::default();

    // Read config file
    let config_path = config_dir().join("config");
    if let Ok(content) = fs::read_to_string(&config_path) {
        let map = parse_ini(&content);

        if let Some(v) = map.get("url") {
            config.url = v.clone();
        }
        if let Some(v) = map.get("width") {
            if let Ok(n) = v.parse::<u32>() {
                config.width = n;
            }
        }
        if let Some(v) = map.get("height") {
            if let Ok(n) = v.parse::<u32>() {
                config.height = n;
            }
        }
        if let Some(v) = map.get("x") {
            if let Ok(n) = v.parse::<i32>() {
                config.x = n;
            }
        }
        if let Some(v) = map.get("y") {
            if let Ok(n) = v.parse::<i32>() {
                config.y = n;
            }
        }
        if let Some(v) = map.get("dev_tools") {
            if let Some(b) = parse_bool(v) {
                config.dev_tools = b;
            }
        }
        if let Some(v) = map.get("start_minimized") {
            if let Some(b) = parse_bool(v) {
                config.start_minimized = b;
            }
        }
        // close_to_tray maps to close_to_background
        if let Some(v) = map.get("close_to_tray") {
            if let Some(b) = parse_bool(v) {
                config.close_to_background = b;
            }
        }
        if let Some(v) = map.get("close_to_background") {
            if let Some(b) = parse_bool(v) {
                config.close_to_background = b;
            }
        }
        if let Some(v) = map.get("frameless") {
            if let Some(b) = parse_bool(v) {
                config.frameless = b;
            }
        }
    }

    // Environment variable overrides
    if let Ok(v) = std::env::var("OFL_URL") {
        config.url = v;
    }
    if let Ok(v) = std::env::var("OFL_WIDTH") {
        if let Ok(n) = v.parse::<u32>() {
            config.width = n;
        }
    }
    if let Ok(v) = std::env::var("OFL_HEIGHT") {
        if let Ok(n) = v.parse::<u32>() {
            config.height = n;
        }
    }
    if let Ok(v) = std::env::var("OFL_DEV_TOOLS") {
        if let Some(b) = parse_bool(&v) {
            config.dev_tools = b;
        }
    }

    config
}

/// Saves window position and size to ~/.config/ofl/window-state
pub fn save_window_state(state: &WindowState) -> std::io::Result<()> {
    let dir = config_dir();
    fs::create_dir_all(&dir)?;
    let path = dir.join("window-state");
    let content = format!(
        "x = {}\ny = {}\nwidth = {}\nheight = {}\n",
        state.x, state.y, state.width, state.height
    );
    fs::write(path, content)
}

/// Loads window state from ~/.config/ofl/window-state
pub fn load_window_state() -> Option<WindowState> {
    let path = config_dir().join("window-state");
    let content = fs::read_to_string(path).ok()?;
    let map = parse_ini(&content);

    Some(WindowState {
        x: map.get("x")?.parse().ok()?,
        y: map.get("y")?.parse().ok()?,
        width: map.get("width")?.parse().ok()?,
        height: map.get("height")?.parse().ok()?,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_ini_basic() {
        let input = "# comment\nurl = https://outlook.office.com\nwidth = 1280\n";
        let map = parse_ini(input);
        assert_eq!(map.get("url").unwrap(), "https://outlook.office.com");
        assert_eq!(map.get("width").unwrap(), "1280");
    }

    #[test]
    fn test_parse_ini_comments() {
        let input = "# hash comment\n; semicolon comment\nkey = value\n";
        let map = parse_ini(input);
        assert_eq!(map.len(), 1);
        assert_eq!(map.get("key").unwrap(), "value");
    }

    #[test]
    fn test_parse_ini_quoted_values() {
        let input = "name = \"hello world\"\nother = 'single quoted'\n";
        let map = parse_ini(input);
        assert_eq!(map.get("name").unwrap(), "hello world");
        assert_eq!(map.get("other").unwrap(), "single quoted");
    }

    #[test]
    fn test_parse_ini_no_spaces() {
        let input = "key=value\n";
        let map = parse_ini(input);
        assert_eq!(map.get("key").unwrap(), "value");
    }

    #[test]
    fn test_parse_bool_variants() {
        assert_eq!(parse_bool("true"), Some(true));
        assert_eq!(parse_bool("True"), Some(true));
        assert_eq!(parse_bool("yes"), Some(true));
        assert_eq!(parse_bool("1"), Some(true));
        assert_eq!(parse_bool("false"), Some(false));
        assert_eq!(parse_bool("False"), Some(false));
        assert_eq!(parse_bool("no"), Some(false));
        assert_eq!(parse_bool("0"), Some(false));
        assert_eq!(parse_bool("invalid"), None);
    }

    #[test]
    fn test_default_config_values() {
        let config = OflConfig::default();
        assert_eq!(config.url, "https://outlook.office.com");
        assert_eq!(config.width, 1280);
        assert_eq!(config.height, 800);
        assert!(!config.dev_tools);
        assert!(!config.start_minimized);
        assert!(config.close_to_background);
        assert!(!config.frameless);
    }
}
