pub mod blocker;
pub mod config;

use config::{load_config, load_window_state, save_window_state, WindowState};
use tauri::{Listener, WebviewUrl, WebviewWindowBuilder};

const USER_AGENT: &str = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36";

pub fn run() {
    let cfg = load_config();
    let close_to_background = cfg.close_to_background;

    // Determine initial window geometry from saved state or config defaults
    let saved = load_window_state();
    let (win_x, win_y, win_width, win_height) = match &saved {
        Some(state) => (state.x, state.y, state.width, state.height),
        None => (cfg.x, cfg.y, cfg.width, cfg.height),
    };

    let start_minimized = cfg.start_minimized;
    let decorations = !cfg.frameless;
    let url = cfg.url.clone();

    tauri::Builder::default()
        .setup(move |app| {
            let external_url: url::Url = url
                .parse()
                .unwrap_or_else(|_| "https://outlook.office.com".parse().unwrap());

            let mut builder =
                WebviewWindowBuilder::new(app, "main", WebviewUrl::External(external_url))
                    .title("Outlook Lite for Linux")
                    .inner_size(win_width as f64, win_height as f64)
                    .decorations(decorations)
                    .visible(!start_minimized)
                    .user_agent(USER_AGENT);

            // Apply saved position if valid (not -1,-1 which means "let WM decide")
            if win_x >= 0 && win_y >= 0 {
                builder = builder.position(win_x as f64, win_y as f64);
            }

            let window = builder
                .on_navigation(|url| {
                    let url_str = url.as_str();

                    // Block ad/tracker domains
                    if blocker::is_blocked(url_str) {
                        return false;
                    }

                    // Allow internal schemes
                    if url_str == "about:blank" || url_str.starts_with("data:") {
                        return true;
                    }

                    // Allowed domains for Outlook auth and app
                    const ALLOWED_DOMAINS: &[&str] = &[
                        "outlook.office.com",
                        "outlook.cloud.microsoft",
                        "outlook.office365.com",
                        "outlook.live.com",
                        "login.microsoftonline.com",
                        "login.live.com",
                        "login.microsoft.com",
                        "aadcdn.msftauth.net",
                        "aadcdn.msauth.net",
                    ];

                    if let Some(host) = url.host_str() {
                        for domain in ALLOWED_DOMAINS {
                            if host == *domain || host.ends_with(&format!(".{}", domain)) {
                                return true;
                            }
                        }
                    }

                    // External link — open in system browser and cancel navigation
                    let _ = open::that(url_str);
                    false
                })
                .build()?;

            // If start_minimized, minimize after build
            if start_minimized {
                let _ = window.minimize();
                let _ = window.show();
            }

            // Handle close_to_background: hide window instead of closing
            let win_close = window.clone();
            window.on_window_event(move |event| {
                if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                    if close_to_background {
                        api.prevent_close();
                        let _ = win_close.hide();
                    }
                }
            });

            // Save window state on move
            let win_moved = window.clone();
            window.listen("tauri://move", move |_| {
                if let Ok(pos) = win_moved.outer_position() {
                    if let Ok(size) = win_moved.inner_size() {
                        let _ = save_window_state(&WindowState {
                            x: pos.x,
                            y: pos.y,
                            width: size.width,
                            height: size.height,
                        });
                    }
                }
            });

            // Save window state on resize
            let win_resized = window.clone();
            window.listen("tauri://resize", move |_| {
                if let Ok(pos) = win_resized.outer_position() {
                    if let Ok(size) = win_resized.inner_size() {
                        let _ = save_window_state(&WindowState {
                            x: pos.x,
                            y: pos.y,
                            width: size.width,
                            height: size.height,
                        });
                    }
                }
            });

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
