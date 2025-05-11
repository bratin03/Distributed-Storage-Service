/*
    CS60002 - Distributed Systems
    Term Project - Spring 2025

    * Author 1: Bratin Mondal (21CS10016)
    * Author 2: Soukhin Nayek (21CS10062)
    * Author 3: Swarnabh Mandal (21CS10068)

    * Department of Computer Science and Engineering
    * Indian Institute of Technology, Kharagpur
*/

#include "../metadata/metadata.hpp"
#include "../fsUtils/fsUtils.hpp"
#include "../logger/Mylogger.hpp"
#include "../login/login.hpp"
#include "../serverUtils/serverUtils.hpp"

namespace boot
{
    void localSync();
    void syncDir(const fs::path &path);
    void localToRemote();
    void localToRemoteDirCheck(const std::string &dir_key);
    void sendDirRecursively(const std::string &dir_key);
    void RemoteToLocal();
    void RemoteToLocalDirCheck(const std::string &dir_key);
    void fetchDirRecursively(const std::string &dir_key);
}