find_package(Boost REQUIRED COMPONENTS system thread)

add_executable(notify_server
    server.cpp
    notification_server.cpp
    http_listener.cpp
)

target_include_directories(notify_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(notify_server
    Boost::system
    Boost::thread
    CURL::libcurl
    OpenSSL::SSL OpenSSL::Crypto
    OpenSSL::SSL OpenSSL::Crypto
    Threads::Threads
    logger
)
