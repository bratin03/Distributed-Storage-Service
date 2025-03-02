/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/
#include <chrono>

int main() {
    typedef std::chrono::steady_clock Clock;
    Clock::time_point tp = Clock::now();
    ((void)tp);
}
