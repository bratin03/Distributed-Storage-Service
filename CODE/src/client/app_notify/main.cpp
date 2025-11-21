/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#include "app_notify.hpp"

int main() {
    // Send a notification. Adjust the title and message as desired.
    if (AppNotify::send_notification("Test Title", "This is a test notification.")) {
        return 0;
    } else {
        return 1;
    }
}
