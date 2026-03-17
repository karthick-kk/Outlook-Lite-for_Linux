#include "badge.h"
#include <gio/gio.h>
#include <cstdio>
#include <string>

static GDBusConnection* g_conn = nullptr;
static std::string g_desktop_uri;

void badge_init(const char* desktop_file) {
    g_desktop_uri = std::string("application://") + desktop_file;

    GError* err = nullptr;
    g_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &err);
    if (!g_conn) {
        fprintf(stderr, "[ofl] Badge: D-Bus session bus unavailable: %s\n",
                err ? err->message : "unknown");
        if (err) g_error_free(err);
        return;
    }
    fprintf(stderr, "[ofl] Badge initialized (%s)\n", g_desktop_uri.c_str());
}

void badge_set_count(int count) {
    if (!g_conn) return;

    GVariantBuilder props;
    g_variant_builder_init(&props, G_VARIANT_TYPE("a{sv}"));

    if (count > 0) {
        g_variant_builder_add(&props, "{sv}", "count",
                              g_variant_new_int64(count));
        g_variant_builder_add(&props, "{sv}", "count-visible",
                              g_variant_new_boolean(TRUE));
    } else {
        g_variant_builder_add(&props, "{sv}", "count-visible",
                              g_variant_new_boolean(FALSE));
    }

    GVariant* params = g_variant_new("(sa{sv})",
                                     g_desktop_uri.c_str(), &props);

    GError* err = nullptr;
    g_dbus_connection_emit_signal(
        g_conn,
        nullptr,                                    // destination (broadcast)
        "/com/canonical/unity/launcherentry/ofl",   // object path
        "com.canonical.Unity.LauncherEntry",        // interface
        "Update",                                   // signal
        params,
        &err);

    if (err) {
        fprintf(stderr, "[ofl] Badge D-Bus error: %s\n", err->message);
        g_error_free(err);
    } else {
        g_dbus_connection_flush_sync(g_conn, nullptr, nullptr);
    }
}

void badge_shutdown() {
    if (g_conn) {
        badge_set_count(0);
        g_object_unref(g_conn);
        g_conn = nullptr;
    }
}
