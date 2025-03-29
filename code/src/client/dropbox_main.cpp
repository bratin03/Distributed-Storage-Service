#include "dropbox/dropbox_client.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

const std::string config_path = "config.json";

int main() {
  nlohmann::json config;
  try {
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
      throw std::runtime_error("Failed to open config file: " + config_path);
    }
    config = nlohmann::json::parse(config_file);
  } catch (const nlohmann::json::parse_error &e) {
    throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
  }

  if (!config.contains("access_token")) {
    throw std::runtime_error("access_token not found in config file");
  }

  rocksdb::DB* raw_db = nullptr;
  if (!config.contains("metadata_database_path")) {
    throw std::runtime_error("metadata_database_path not found in config file");
  }

  std::string db_path = config["metadata_database_path"];
  rocksdb::Options options;
  options.create_if_missing = true;

  rocksdb::Status status = rocksdb::DB::Open(options, db_path, &raw_db);
  if (!status.ok()) {
    throw std::runtime_error("Failed to open RocksDB at path: " + db_path +
                             " Error: " + status.ToString());
  }

  std::shared_ptr<rocksdb::DB> db(raw_db, [](rocksdb::DB* ptr) { delete ptr; });

  bootup_1(config, db);

  return 0;
}
