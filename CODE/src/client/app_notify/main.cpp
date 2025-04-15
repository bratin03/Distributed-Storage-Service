#include "app_notify.hpp"

int main()
{
    // Send a notification. Adjust the title and message as desired.
    if (AppNotify::send_notification("Test Title", "This is a test notification."))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}
