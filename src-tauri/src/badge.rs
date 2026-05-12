use zbus::{Connection, Result};
use zbus::zvariant::{SerializeDict, Type};
use once_cell::sync::OnceCell;
use std::sync::{Arc, Mutex};

/// Properties dict for the Unity LauncherEntry Update signal.
#[derive(SerializeDict, Type)]
#[zvariant(signature = "a{sv}")]
struct LauncherEntryProps {
    count: i64,
    #[zvariant(rename = "count-visible")]
    count_visible: bool,
}

pub struct BadgeService {
    connection: Connection,
}

const DESKTOP_URI: &str = "application://ofl.desktop";
const OBJECT_PATH: &str = "/com/canonical/unity/launcherentry/ofl";
const INTERFACE: &str = "com.canonical.Unity.LauncherEntry";

static GLOBAL_BADGE: OnceCell<Arc<Mutex<Option<Connection>>>> = OnceCell::new();

impl BadgeService {
    pub async fn init() -> Result<Self> {
        let connection = Connection::session().await?;
        // Store a clone in the global for protocol handler access
        let global = GLOBAL_BADGE.get_or_init(|| Arc::new(Mutex::new(None)));
        *global.lock().unwrap() = Some(connection.clone());
        Ok(Self { connection })
    }

    pub async fn set_count(&self, count: u32) -> Result<()> {
        let props = LauncherEntryProps {
            count: count as i64,
            count_visible: count > 0,
        };

        self.connection
            .emit_signal(
                None::<&str>,
                OBJECT_PATH,
                INTERFACE,
                "Update",
                &(DESKTOP_URI, props),
            )
            .await?;

        Ok(())
    }
}

/// Called from the IPC protocol handler (no access to Tauri state)
pub async fn set_count_global(count: u32) {
    let global = match GLOBAL_BADGE.get() {
        Some(g) => g,
        None => return,
    };
    let conn = {
        let guard = global.lock().unwrap();
        guard.clone()
    };
    if let Some(conn) = conn.as_ref() {
        let props = LauncherEntryProps {
            count: count as i64,
            count_visible: count > 0,
        };
        let _ = conn
            .emit_signal(
                None::<&str>,
                OBJECT_PATH,
                INTERFACE,
                "Update",
                &(DESKTOP_URI, props),
            )
            .await;
    }
}
