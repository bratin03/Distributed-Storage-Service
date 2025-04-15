#pragma once
#include <string>

namespace AppNotify
{
    // Sends a notification with the given title and message.
    // Returns true if successful, false otherwise.
    bool send_notification(const std::string &title, const std::string &message);
}
