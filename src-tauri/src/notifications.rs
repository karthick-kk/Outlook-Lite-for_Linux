use notify_rust::{Notification, Timeout};

/// Show a desktop notification for a new mail.
pub fn show(sender: &str, subject: &str) {
    if let Err(e) = Notification::new()
        .summary(sender)
        .body(subject)
        .icon("ofl-app")
        .timeout(Timeout::Milliseconds(5000))
        .show()
    {
        eprintln!("notification error: {e}");
    }
}

/// No-op init (notify-rust doesn't need initialization).
pub fn init() {}
