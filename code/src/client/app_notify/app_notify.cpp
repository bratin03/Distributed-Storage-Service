#include "app_notify.hpp"
#include <libnotify/notify.h>
#include <iostream>

namespace AppNotify
{

    bool send_notification(const std::string &title, const std::string &message)
    {
        // Initialize libnotify with a default application name.
        if (!notify_init("AppNotify"))
        {
            std::cerr << "Failed to initialize libnotify." << std::endl;
            return false;
        }

        // Create a new notification object. Passing nullptr as the icon uses the default.
        NotifyNotification *notification = notify_notification_new(title.c_str(), message.c_str(), nullptr);
        GError *error = nullptr;

        // Display the notification.
        bool result = notify_notification_show(notification, &error);
        if (!result && error)
        {
            std::cerr << "Error showing notification: " << error->message << std::endl;
            g_error_free(error);
            g_object_unref(G_OBJECT(notification));
            notify_uninit();
            return false;
        }

        // Free the notification object.
        g_object_unref(G_OBJECT(notification));
        // Uninitialize libnotify.
        notify_uninit();
        return true;
    }

} // namespace AppNotify
