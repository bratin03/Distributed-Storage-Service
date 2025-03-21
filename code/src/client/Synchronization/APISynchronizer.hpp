#pragma once
#include <string>
#include <vector>

// APISynchronizer encapsulates calls to the REST endpoints for
// directory management, file operations, and file updates.
class APISynchronizer {
public:
    // DIRECTORY APIs
    bool createDirectory(const std::string& dir_id);
    bool deleteDirectory(const std::string& dir_id);
    std::string listDirectory(const std::string& dir_id); // returns JSON response

    // FILE APIs
    bool deleteFile(const std::string& file_id);
    std::string downloadFile(const std::string& file_id);  // returns metadata JSON
    std::string downloadChunk(const std::string& chunk_id); // returns JSON with metadata and data

    // UPDATE FILE APIs
    // updateRequest sends file_version, file metadata (as JSON string) and a list of chunk IDs,
    // then returns the list of required chunks in JSON.
    std::string updateRequest(const std::string& file_version, 
                              const std::string& file_metadata, 
                              const std::vector<std::string>& chunkIDs);
    
    // storeChunk uploads a single chunk (its id, metadata as JSON, and data as string).
    bool storeChunk(const std::string& chunkid, 
                    const std::string& metadata, 
                    const std::string& data);
    
    // commitUpdate tells the server to finalize the update with file metadata and chunk IDs.
    // It returns a response that is either 200OK or (if some chunk is missing) the missing chunkid.
    std::string commitUpdate(const std::string& file_metadata, 
                             const std::vector<std::string>& chunkIDs);
};
