#pragma once

#include <string>

class Notification {
public:
    /**
     * Constructs a Notification object with the given application name.
     * Initializes the libnotify library.
     */
    Notification(const std::string &app_name);

    /**
     * Destructor that uninitializes libnotify.
     */
    ~Notification();

    /**
     * Sends a notification with the specified title, message, and icon.
     *
     * @param title   The title of the notification.
     * @param message The body text of the notification.
     * @param icon    The icon to display (default: "dialog-information").
     * @return true if the notification was displayed successfully; false otherwise.
     */
    bool send(const std::string &title, const std::string &message, const std::string &icon = "dialog-information");

private:
    // You can add private members or helper functions if needed.
};

