#include <iostream>
#include <string>
#include "leveldb/db.h"      // LevelDB header
#include "nlohmann/json.hpp" // JSON library header

using json = nlohmann::json;

int main()
{
    // Open (or create) a LevelDB database at the specified path.
    leveldb::DB *db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    std::string dbPath = "testdb";
    leveldb::Status status = leveldb::DB::Open(options, dbPath, &db);
    if (!status.ok())
    {
        std::cerr << "Unable to open/create database at " << dbPath << "\n";
        std::cerr << status.ToString() << "\n";
        return 1;
    }
    std::cout << "Database opened successfully.\n";

    // Create some JSON objects.
    json user1 = {{"name", "Alice"}, {"age", 30}};
    json user2 = {{"name", "Bob"}, {"age", 25}};
    json admin1 = {{"name", "Charlie"}, {"age", 35}};
    json user3 = {{"name", "Dave"}, {"age", 40}};

    // Insert the JSON objects into the database as strings.
    // We use keys with a prefix (e.g., "user:" or "admin:") for easy prefix scans.
    status = db->Put(leveldb::WriteOptions(), "user:1", user1.dump());
    if (!status.ok())
    {
        std::cerr << "Put failed for key user:1\n";
    }
    status = db->Put(leveldb::WriteOptions(), "user:2", user2.dump());
    if (!status.ok())
    {
        std::cerr << "Put failed for key user:2\n";
    }
    status = db->Put(leveldb::WriteOptions(), "admin:1", admin1.dump());
    if (!status.ok())
    {
        std::cerr << "Put failed for key admin:1\n";
    }
    status = db->Put(leveldb::WriteOptions(), "user:3", user3.dump());
    if (!status.ok())
    {
        std::cerr << "Put failed for key user:3\n";
    }

    // Retrieve and parse a JSON object from the database.
    std::string retrievedValue;
    status = db->Get(leveldb::ReadOptions(), "user:1", &retrievedValue);
    if (status.ok())
    {
        json retrievedJson = json::parse(retrievedValue);
        std::cout << "Retrieved key 'user:1': " << retrievedJson.dump() << "\n";
    }
    else
    {
        std::cerr << "Get failed for key user:1\n";
    }

    // Perform a prefix scan: iterate over all keys that start with "user:".
    std::string prefix = "user:";
    leveldb::Iterator *it = db->NewIterator(leveldb::ReadOptions());
    std::cout << "\nPrefix scan for keys starting with \"" << prefix << "\":\n";
    for (it->Seek(prefix);
         it->Valid() && it->key().ToString().substr(0, prefix.size()) == prefix;
         it->Next())
    {
        std::string key = it->key().ToString();
        std::string value = it->value().ToString();
        json j = json::parse(value);
        std::cout << key << " => " << j.dump() << "\n";
    }
    if (!it->status().ok())
    {
        std::cerr << "Iterator error: " << it->status().ToString() << "\n";
    }
    delete it;

    // Clean up and close the database.
    delete db;
    return 0;
}
