# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 \
	-I../../utils/libraries/cpp-httplib \
	-I../../utils/libraries/jwt-cpp/include \
	-I/usr/include/nlohmann \
	-I./logger

LDFLAGS = -L/usr/lib -lssl -lcrypto -lpthread -lhiredis -lcurl -g -pg -O2

# Targets
METASERVER_TARGET = metaserver.out

# Source files
METASERVER_SRC = metadata_server_L1.cpp authentication/authentication.cpp initiation/initiation.cpp database_handler/database_handler.cpp

# Compile MetaServer
$(METASERVER_TARGET): $(METASERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean target
clean:
	rm -f $(METASERVER_TARGET)
