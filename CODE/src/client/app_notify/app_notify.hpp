/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#pragma once
#include <string>

namespace AppNotify
{
    // Sends a notification with the given title and message.
    // Returns true if successful, false otherwise.
    bool send_notification(const std::string &title, const std::string &message);
}
