#include <libnotify/notify.h>

int main(int argc, char **argv)
{
    // Initialize the libnotify system with your application name.
    if (!notify_init("MyApp"))
    {
        return 1; // If initialization fails, exit with an error code.
    }

    // Create a new notification.
    // Parameters: notification title, message body, and an icon name (or path to an image file).
    NotifyNotification *notification = notify_notification_new(
        "Hello!",                                         // Title
        "This is a notification from a C++ application.", // Body text
        "dialog-information"                              // Icon (using a standard icon name)
    );

    // Display the notification.
    GError *error = nullptr;
    if (!notify_notification_show(notification, &error))
    {
        // Check if an error occurred.
        if (error)
        {
            g_printerr("Error displaying notification: %s\n", error->message);
            g_error_free(error);
        }
        g_object_unref(G_OBJECT(notification));
        notify_uninit();
        return 1;
    }

    // Free the notification object after use.
    g_object_unref(G_OBJECT(notification));

    // Uninitialize libnotify before exiting.
    notify_uninit();

    return 0;
}
