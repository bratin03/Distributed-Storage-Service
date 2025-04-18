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