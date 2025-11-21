# Header-only libraries are already included via include_directories in root
# But we can define interface targets if we want to be more modern.
# For now, just keeping it empty or minimal as we handled includes in root.
# Actually, let's define interface libraries.

add_library(cpp-httplib INTERFACE)
target_include_directories(cpp-httplib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib)

add_library(jwt-cpp INTERFACE)
target_include_directories(jwt-cpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/jwt-cpp/include)
