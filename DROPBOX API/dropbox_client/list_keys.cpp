#include "rocksdb/db.h"
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <path_to_db>" << std::endl;
    return 1;
  }

  std::string db_path = argv[1];
  rocksdb::DB *db;
  rocksdb::Options options;
  options.create_if_missing = false; // Expect an existing DB

  // Open the RocksDB instance
  rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db);
  if (!status.ok()) {
    std::cerr << "Unable to open DB at " << db_path << ": " << status.ToString()
              << std::endl;
    return 1;
  }

  // Create an iterator to traverse the keys
  rocksdb::Iterator *it = db->NewIterator(rocksdb::ReadOptions());
  std::cout << "Keys in the database:" << std::endl;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    // Print each key as a string
    std::cout << it->key().ToString() << std::endl;
  }

  // Check for any errors found during the iteration
  if (!it->status().ok()) {
    std::cerr << "Error during iteration: " << it->status().ToString()
              << std::endl;
  }

  // Clean up resources
  delete it;
  delete db;
  return 0;
}
