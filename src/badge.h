#pragma once

// Set the dock/launcher badge count via D-Bus (Unity LauncherEntry API).
// Supported by GNOME (with dash-to-dock/Ubuntu dock), KDE, Budgie, etc.
void badge_init(const char* desktop_file);  // e.g. "ofl.desktop"
void badge_set_count(int count);
void badge_shutdown();
