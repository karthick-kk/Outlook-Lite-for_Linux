pub mod badge;
pub mod blocker;
pub mod config;
pub mod notifications;

use config::{load_config, load_window_state, save_window_state, WindowState};
use tauri::{Listener, Manager, WebviewUrl, WebviewWindowBuilder};
use std::sync::atomic::{AtomicI32, Ordering};
use webkit2gtk::WebViewExt;
use webkit2gtk::NavigationPolicyDecision;
use webkit2gtk::NavigationPolicyDecisionExt;
use webkit2gtk::PolicyDecisionExt;
use webkit2gtk::PolicyDecisionType;
use webkit2gtk::URIRequestExt;
use webkit2gtk::glib;
use webkit2gtk::gio;
use java_script_core::ValueExt;

static LAST_BADGE: AtomicI32 = AtomicI32::new(-1);

pub struct BadgeState(badge::BadgeService);
pub struct DevToolsEnabled(bool);

#[tauri::command]
fn new_mail(sender: String, subject: String) {
    notifications::show(&sender, &subject);
}

#[tauri::command]
async fn update_badge(count: u32, state: tauri::State<'_, BadgeState>) -> Result<(), String> {
    state.0.set_count(count).await.map_err(|e| e.to_string())
}

#[tauri::command]
fn reload(window: tauri::WebviewWindow) {
    let _ = window.eval("window.location.reload()");
}

#[tauri::command]
fn reload_no_cache(window: tauri::WebviewWindow) {
    let _ = window.eval("window.location.reload()");
}

#[tauri::command]
fn quit(app: tauri::AppHandle) {
    app.exit(0);
}

#[tauri::command]
fn toggle_devtools(window: tauri::WebviewWindow, state: tauri::State<'_, DevToolsEnabled>) {
    if !state.0 {
        return;
    }
    if window.is_devtools_open() {
        window.close_devtools();
    } else {
        window.open_devtools();
    }
}

const USER_AGENT: &str = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36";

/// Simple JSON field extractor for flat objects
fn extract_json_field(json: &str, key: &str) -> String {
    let needle = format!("\"{}\":", key);
    let pos = match json.find(&needle) {
        Some(p) => p + needle.len(),
        None => return String::new(),
    };
    let rest = json[pos..].trim_start();
    if rest.starts_with('"') {
        let start = 1;
        let mut end = start;
        let bytes = rest.as_bytes();
        while end < bytes.len() {
            if bytes[end] == b'"' && (end == 0 || bytes[end - 1] != b'\\') {
                break;
            }
            end += 1;
        }
        rest[start..end].to_string()
    } else {
        let end = rest.find(|c: char| c == ',' || c == '}' || c == ' ').unwrap_or(rest.len());
        rest[..end].to_string()
    }
}

pub fn run() {
    let cfg = load_config();
    let close_to_background = cfg.close_to_background;
    let dev_tools = cfg.dev_tools;

    let saved = load_window_state();
    let (win_x, win_y, win_width, win_height) = match &saved {
        Some(state) => (state.x, state.y, state.width, state.height),
        None => (cfg.x, cfg.y, cfg.width, cfg.height),
    };

    let start_minimized = cfg.start_minimized;
    let decorations = !cfg.frameless;
    let url = cfg.url.clone();

    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![new_mail, update_badge, reload, reload_no_cache, quit, toggle_devtools])
        .setup(move |app| {
            app.manage(DevToolsEnabled(dev_tools));

            let badge_service = tauri::async_runtime::block_on(async {
                badge::BadgeService::init().await
            });
            match badge_service {
                Ok(svc) => { app.manage(BadgeState(svc)); }
                Err(e) => eprintln!("[ofl] Badge service unavailable: {}", e),
            }

            let external_url: url::Url = url
                .parse()
                .unwrap_or_else(|_| "https://outlook.office.com".parse().unwrap());

            let mut builder =
                WebviewWindowBuilder::new(app, "main", WebviewUrl::External(external_url))
                    .inner_size(win_width as f64, win_height as f64)
                    .decorations(decorations)
                    .visible(!start_minimized)
                    .user_agent(USER_AGENT);

            // Custom CSS injection
            let custom_css_path = config::config_dir().join("custom.css");
            if custom_css_path.exists() {
                if let Ok(css_content) = std::fs::read_to_string(&custom_css_path) {
                    let escaped = css_content.replace('`', "\\`").replace("${", "\\${");
                    let css_js = format!(
                        r#"(function() {{
                            var s = document.createElement('style');
                            s.textContent = `{}`;
                            document.head.appendChild(s);
                        }})();"#,
                        escaped
                    );
                    builder = builder.initialization_script(&css_js);
                }
            }

            if win_x >= 0 && win_y >= 0 {
                builder = builder.position(win_x as f64, win_y as f64);
            }

            let window = builder
                .on_navigation(|url| {
                    let url_str = url.as_str();

                    if blocker::is_blocked(url_str) {
                        return false;
                    }

                    if url_str == "about:blank" || url_str.starts_with("data:") {
                        return true;
                    }

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

                    let _ = open::that(url_str);
                    false
                })
                .build()?;

            // Handle new-window requests (target="_blank" links) via WebKitGTK signal
            let poll_window_nav = window.clone();
            let _ = window.with_webview(|webview| {
                use glib::object::ObjectExt;
                use glib::Cast;
                let wv = webview.inner();
                wv.connect("decide-policy", false, |values| {
                    let decision = values[1].get::<webkit2gtk::PolicyDecision>().unwrap();
                    let decision_type = values[2].get::<PolicyDecisionType>().unwrap();

                    if decision_type == PolicyDecisionType::NewWindowAction {
                        let nav_decision: NavigationPolicyDecision = unsafe { decision.unsafe_cast() };
                        if let Some(request) = nav_decision.request() {
                            if let Some(uri) = request.uri() {
                                let url = uri.to_string();
                                if !url.is_empty() {
                                    // Check if it's an auth/Outlook domain — navigate in-webview
                                    let is_internal = [
                                        "outlook.office.com",
                                        "outlook.cloud.microsoft",
                                        "outlook.office365.com",
                                        "outlook.live.com",
                                        "login.microsoftonline.com",
                                        "login.live.com",
                                        "login.microsoft.com",
                                        "aadcdn.msftauth.net",
                                        "aadcdn.msauth.net",
                                    ].iter().any(|domain| {
                                        url.contains(domain)
                                    });

                                    if is_internal {
                                        // Use the webview from the signal source
                                        let source_wv = values[0].get::<webkit2gtk::WebView>().unwrap();
                                        source_wv.load_uri(&url);
                                    } else {
                                        let _ = open::that(&url);
                                    }
                                }
                            }
                        }
                        nav_decision.ignore();
                        return Some(true.into());
                    }
                    Some(false.into())
                });
            });

            if start_minimized {
                let _ = window.minimize();
                let _ = window.show();
            }

            // Close-to-background
            let win_close = window.clone();
            window.on_window_event(move |event| {
                if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                    if close_to_background {
                        api.prevent_close();
                        let _ = win_close.hide();
                    }
                }
            });

            // Persist window state
            let win_moved = window.clone();
            window.listen("tauri://move", move |_| {
                if let Ok(pos) = win_moved.outer_position() {
                    if let Ok(size) = win_moved.inner_size() {
                        let _ = save_window_state(&WindowState {
                            x: pos.x, y: pos.y,
                            width: size.width, height: size.height,
                        });
                    }
                }
            });
            let win_resized = window.clone();
            window.listen("tauri://resize", move |_| {
                if let Ok(pos) = win_resized.outer_position() {
                    if let Ok(size) = win_resized.inner_size() {
                        let _ = save_window_state(&WindowState {
                            x: pos.x, y: pos.y,
                            width: size.width, height: size.height,
                        });
                    }
                }
            });

            // JS injection + polling via WebKitGTK evaluate_javascript
            let poll_window = window.clone();
            let inject_js = include_str!("../../src/inject.js").to_string();
            std::thread::spawn(move || {
                // Wait for page to fully load
                std::thread::sleep(std::time::Duration::from_secs(10));
                let _ = poll_window.eval(&inject_js);

                let mut last_inject = std::time::Instant::now();
                let mut last_badge: i32 = -1;

                loop {
                    std::thread::sleep(std::time::Duration::from_secs(3));

                    // Re-inject on navigation (every 60s)
                    if last_inject.elapsed() > std::time::Duration::from_secs(60) {
                        let _ = poll_window.eval(&inject_js);
                        last_inject = std::time::Instant::now();
                    }

                    // Poll JS state via webkit2gtk callback
                    let last_badge_clone = last_badge;
                    let _ = poll_window.with_webview(move |webview| {
                        let wv = webview.inner();
                        let js = r#"
                            (function() {
                                var s = window.__ofl_state;
                                if (!s) return '';
                                var result = {
                                    n: s.notifications.splice(0, s.notifications.length),
                                    b: s.badgeCount,
                                    u: (s.pendingUrls || []).splice(0, (s.pendingUrls || []).length),
                                    q: !!s.quit
                                };
                                return JSON.stringify(result);
                            })()
                        "#;

                        wv.evaluate_javascript(js, None, None, None::<&gio::Cancellable>, move |result: Result<java_script_core::Value, glib::Error>| {
                            if let Ok(value) = result {
                                let json_str = value.to_string();
                                let s = json_str.as_str();
                                if s.is_empty() { return; }

                                // Process notifications
                                if let Some(n_start) = s.find("\"n\":[") {
                                    let rest = &s[n_start + 4..];
                                    if let Some(arr_end) = rest.find(']') {
                                        let arr = &rest[1..arr_end];
                                        if !arr.is_empty() {
                                            for obj in arr.split("},{") {
                                                let obj = obj.trim_start_matches('{').trim_end_matches('}');
                                                let sender = extract_json_field(obj, "sender");
                                                let subject = extract_json_field(obj, "subject");
                                                if !sender.is_empty() {
                                                    notifications::show(&sender, &subject);
                                                }
                                            }
                                        }
                                    }
                                }

                                // Process badge
                                if let Some(b_start) = s.find("\"b\":") {
                                    let rest = &s[b_start + 4..];
                                    let end = rest.find(|c: char| c == ',' || c == '}').unwrap_or(rest.len());
                                    if let Ok(badge) = rest[..end].trim().parse::<i32>() {
                                        if badge != last_badge_clone && badge >= 0 {
                                            LAST_BADGE.store(badge, Ordering::Relaxed);
                                            let count = badge as u32;
                                            tauri::async_runtime::spawn(async move {
                                                badge::set_count_global(count).await;
                                            });
                                        }
                                    }
                                }

                                // Process pending URLs (open in system browser)
                                if let Some(u_start) = s.find("\"u\":[") {
                                    let rest = &s[u_start + 4..];
                                    if let Some(arr_end) = rest.find(']') {
                                        let arr = &rest[1..arr_end];
                                        if !arr.is_empty() {
                                            for url in arr.split("\",\"") {
                                                let url = url.trim_start_matches('"').trim_end_matches('"');
                                                if !url.is_empty() {
                                                    let _ = open::that(url);
                                                }
                                            }
                                        }
                                    }
                                }

                                // Process quit
                                if s.contains("\"q\":true") {
                                    std::process::exit(0);
                                }
                            }
                        });
                    });

                    last_badge = LAST_BADGE.load(Ordering::Relaxed);
                }
            });

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
