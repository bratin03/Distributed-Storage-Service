#include "app_notify.hpp"
#include <libnotify/notify.h>
#include <iostream>

Notification::Notification(const std::string &app_name) {
    if (!notify_init(app_name.c_str())) {
        std::cerr << "Failed to initialize libnotify." << std::endl;
    }
}

Notification::~Notification() {
    notify_uninit();
}

bool Notification::send(const std::string &title, const std::string &message, const std::string &icon) {
    // Create a new notification object
    NotifyNotification *notification = notify_notification_new(title.c_str(), message.c_str(), icon.c_str());
    GError *error = nullptr;
    
    // Send the notification
    bool result = notify_notification_show(notification, &error);
    if (!result && error) {
        std::cerr << "Error showing notification: " << error->message << std::endl;
        g_error_free(error);
        g_object_unref(G_OBJECT(notification));
        return false;
    }
    // Free notification object
    g_object_unref(G_OBJECT(notification));
    return true;
}
