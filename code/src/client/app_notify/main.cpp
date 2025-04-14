#include "app_notify.hpp"

int main() {
    // Create a Notification object with the application name "MyApp"
    Notification notif("MyApp");

    // Send a test notification
    if (notif.send("Test Notification", "This is a test notification from the Notification library.")) {
        return 0;
    } else {
        return 1;
    }
}
