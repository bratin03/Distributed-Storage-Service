# Client CMakeLists.txt

# Define helper libraries
add_library(boot boot/boot.cpp)
add_library(cache cache/cache.cpp) # Check filename
add_library(fsUtils fsUtils/fsUtils.cpp) # Check filename
add_library(load_config load_config/load_config.cpp) # Check filename
add_library(login login/login.cpp) # Check filename
add_library(merge merge/merge.cpp) # Check filename
add_library(metadata metadata/metadata.cpp) # Check filename
add_library(serverUtils serverUtils/serverUtils.cpp) # Check filename
add_library(notification notification/notification.cpp) # Check filename
add_library(watcher watcher/watcher.cpp) # Check filename
add_library(kv kv/kv.cpp) # Check filename
add_library(process_local process_local/process_local.cpp) # Check filename
add_library(process_remote process_remote/process_remote.cpp) # Check filename
add_library(app_notify app_notify/app_notify.cpp) # Check filename

# Include directories for libraries
target_include_directories(boot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/boot)
target_include_directories(cache PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/cache)
target_include_directories(fsUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/fsUtils)
target_include_directories(load_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/load_config)
target_include_directories(login PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/login)
target_include_directories(merge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/merge)
target_include_directories(metadata PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/metadata)
target_include_directories(serverUtils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/serverUtils)
target_include_directories(notification PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/notification)
target_include_directories(watcher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/watcher)
target_include_directories(kv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/kv)
target_include_directories(process_local PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/process_local)
target_include_directories(process_remote PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/process_remote)
target_include_directories(app_notify PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/app_notify)

target_link_libraries(boot PUBLIC logger)
target_link_libraries(cache PUBLIC logger)
target_link_libraries(fsUtils PUBLIC logger)
target_link_libraries(load_config PUBLIC logger)
target_link_libraries(login PUBLIC logger)
target_link_libraries(merge PUBLIC logger)
target_link_libraries(metadata PUBLIC logger)
target_link_libraries(notification PUBLIC logger)
target_link_libraries(watcher PUBLIC logger)
target_link_libraries(kv PUBLIC logger)
target_link_libraries(process_local PUBLIC logger)
target_link_libraries(process_remote PUBLIC logger)
target_link_libraries(app_notify PUBLIC logger)

# Client executable
add_executable(client client.cpp)

# Link libraries
target_link_libraries(serverUtils PUBLIC
    cache
    login
    metadata
    fsUtils
    kv
    merge
    app_notify
    logger
)

target_link_libraries(client
    serverUtils # Depends on many others, so put it first-ish (or let cmake handle it via transitive deps)
    boot
    process_local
    process_remote
    notification
    watcher
    # The following are transitive deps of serverUtils, but we can list them to be explicit or if client uses them directly
    cache
    fsUtils
    load_config
    login
    merge
    metadata
    kv
    app_notify
    cpp-httplib
    jwt-cpp
    OpenSSL::SSL OpenSSL::Crypto
    Threads::Threads
    CURL::libcurl
    ${LIBNOTIFY_LIBRARIES}
    ${GLIB_LIBRARIES}
    rocksdb
    z
    git2
    logger
)

# Add include directories for client
target_include_directories(client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBNOTIFY_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)
