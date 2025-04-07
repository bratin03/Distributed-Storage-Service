#include <iostream>
#include "rocksdb/db.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_db>" << std::endl;
        return 1;
    }
    
    std::string db_path = argv[1];
    rocksdb::Options options;
    // Note: Adjust options if needed
    
    // Destroy the database at the specified path
    rocksdb::Status status = rocksdb::DestroyDB(db_path, options);
    if (!status.ok()) {
        std::cerr << "Error deleting database: " << status.ToString() << std::endl;
        return 1;
    }
    
    std::cout << "Database deleted successfully." << std::endl;
    return 0;
}
