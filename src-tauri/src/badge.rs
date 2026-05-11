use zbus::{Connection, Result};
use zbus::zvariant::{SerializeDict, Type};

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

impl BadgeService {
    pub async fn init() -> Result<Self> {
        let connection = Connection::session().await?;
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
