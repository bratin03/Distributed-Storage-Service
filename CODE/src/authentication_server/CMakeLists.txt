add_executable(auth_server authentication_server.cpp)

target_include_directories(auth_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/logger)

target_link_libraries(auth_server
    cpp-httplib
    jwt-cpp
    OpenSSL::SSL OpenSSL::Crypto
    Threads::Threads
    CURL::libcurl
    hiredis
    logger
)
