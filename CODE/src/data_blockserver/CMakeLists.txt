add_executable(data_block_client client.cpp)

target_include_directories(data_block_client PRIVATE
    ${CMAKE_SOURCE_DIR}/utils/Distributed_KV/client_lib
)

target_link_libraries(data_block_client
    cpp-httplib
    CURL::libcurl
)
