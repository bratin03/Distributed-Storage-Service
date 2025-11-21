# Metadata Server CMakeLists.txt

# Define helper libraries (assuming they exist in subdirectories)
# Check file structure first. metaserver has subdirectories:
# authentication, database_handler, deletion_manager, initiation, logger, nginx

add_library(meta_auth authentication/authentication.cpp)
add_library(meta_db database_handler/database_handler.cpp)
add_library(meta_del deletion_manager/deletion_manager.cpp)
add_library(meta_init initiation/initiation.cpp)

target_include_directories(meta_auth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/authentication)
target_include_directories(meta_db PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/database_handler)
target_include_directories(meta_del PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/deletion_manager)
target_include_directories(meta_init PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/initiation)

# Header-only logger
target_include_directories(meta_init PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/logger) # Initiation uses logger?
# Actually, let's just add logger include to metaserver and libs that use it.
# Assuming all libs might use logger.

add_executable(metaserver metadata_server_L1.cpp)

target_include_directories(metaserver PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/logger)

target_link_libraries(metaserver
    meta_auth
    meta_db
    meta_del
    meta_init
    cpp-httplib
    jwt-cpp
    OpenSSL::SSL OpenSSL::Crypto
    Threads::Threads
    CURL::libcurl
    CURL::libcurl
    hiredis
    logger
)

target_link_libraries(meta_auth logger)
target_link_libraries(meta_db logger)
target_link_libraries(meta_del logger)
target_link_libraries(meta_init logger)
