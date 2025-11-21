add_executable(signup_server signup_server.cpp)

target_link_libraries(signup_server
    cpp-httplib
    CURL::libcurl
    hiredis
    Threads::Threads
    logger
)
