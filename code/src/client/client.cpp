#include "load_config/load_config.hpp"
#include "login/login.hpp"
#include "nlohmann/json.hpp"
#include "metadata/metadata.hpp"
#include "fsUtils/fsUtils.hpp"
#include <iostream>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include "logger/Mylogger.hpp"
#include "boot/boot.hpp"
#include <queue>
#include "serverUtils/serverUtils.hpp"
#include "notification/notification.hpp"
#include "watcher/watcher.hpp"
#include "process_local/process_local.hpp"

using json = nlohmann::json;

// Global constants
const std::string user_info_config_path = "config/user_info.json";
const std::string user_config_path = "config/user_config.json";
json user_info;
json user_config;
json server_info;
std::shared_ptr<rocksdb::DB> db_instance = nullptr;

int main(int argc, char *argv[])
{
    // Load server and user info configurations.
    login::handle_server_info(server_info);
    login::handle_user_info(argc, argv, user_info, user_info_config_path);
    user_info = ConfigReader::load(user_config_path);

    // Get the metadata path from configuration.
    auto metadata_path = ConfigReader::get_config_string("metadata_path", user_info);
    MyLogger::info("Metadata path: " + metadata_path);

    // Initialize RocksDB using the factory method instead of direct instantiation.
    rocksdb::DB *raw_db = nullptr;
    rocksdb::Options options;
    options.create_if_missing = true;
    rocksdb::Status status = rocksdb::DB::Open(options, metadata_path, &raw_db);
    if (!status.ok())
    {
        std::cerr << "Failed to open RocksDB: " << status.ToString() << std::endl;
        return 1;
    }
    db_instance = std::shared_ptr<rocksdb::DB>(raw_db);

    // Pass the database instance to the metadata module.
    metadata::setDatabase(db_instance);

    // Initialize file system utilities.
    auto monitoring_path = ConfigReader::get_config_string("monitoring_path", user_info);
    serverUtils::device_id = ConfigReader::get_config_string("device_id", user_info);
    process_local::wait_threshold = ConfigReader::get_config_value("wait_threshold", user_info);
    process_local::wait_time = ConfigReader::get_config_value("wait_time", user_info);

    fsUtils::initialize(monitoring_path, login::username);

    serverUtils::initializeCache();

    boot::localSync();

    boot::RemoteToLocal();

    boot::localToRemote();

    std::mutex db_mutex;

    std::queue<json> notification_queue;
    std::mutex queue_mutex;

    notification::NotificationClient notification_client(
        serverUtils::notificationLoadBalancerip,
        serverUtils::notificationLoadBalancerPort,
        login::username,
        notification_queue,
        queue_mutex);

    notification_client.start();

    std::queue<watcher::FileEvent> eventQueue;
    std::set<watcher::FileEvent> eventMap;
    std::mutex watcher_mtx;
    std::condition_variable cv;

    std::thread process_thread([&]()
                               { process_local::process_local_events(eventQueue, eventMap, watcher_mtx, cv, db_mutex); });

    // Create watcher thread
    std::thread watcher_thread([&]()
                               { watcher::watch_directory(monitoring_path, eventQueue, eventMap, watcher_mtx, cv); });

    watcher_thread.join();
    process_thread.join();

    return 0;
}
