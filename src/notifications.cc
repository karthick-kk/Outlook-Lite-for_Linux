#include "notifications.h"
#include <libnotify/notify.h>
#include <cstdio>

static std::function<void()> g_click_handler;

static void on_notification_action(NotifyNotification*, char*, gpointer) {
    if (g_click_handler) {
        g_click_handler();
    }
}

void notifications_init() {
    notify_init("Outlook Lite for Linux");
    fprintf(stderr, "[ofl] Notifications initialized\n");
}

void notifications_set_click_handler(std::function<void()> handler) {
    g_click_handler = std::move(handler);
}

void notifications_show(const std::string& title, const std::string& body) {
    NotifyNotification* n = notify_notification_new(
        title.c_str(), body.c_str(), "ofl");
    notify_notification_set_urgency(n, NOTIFY_URGENCY_NORMAL);
    notify_notification_set_timeout(n, 5000);

    notify_notification_add_action(n, "default", "Show",
        on_notification_action, nullptr, nullptr);

    GError* error = nullptr;
    if (!notify_notification_show(n, &error)) {
        fprintf(stderr, "[ofl] Notification error: %s\n",
                error ? error->message : "unknown");
        if (error) g_error_free(error);
    }
    g_object_unref(G_OBJECT(n));
}

void notifications_shutdown() {
    g_click_handler = nullptr;
    notify_uninit();
}
