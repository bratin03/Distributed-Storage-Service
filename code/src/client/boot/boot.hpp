#include "../metadata/metadata.hpp"
#include "../fsUtils/fsUtils.hpp"
#include "../logger/Mylogger.hpp"

namespace boot
{
    void localSync();
    void syncDir(const fs::path &path);
}