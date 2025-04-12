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
AUTHSERVER_TARGET = auth_server
NOFYSERVER_TARGET = notify_server

# Source files
METASERVER_SRC = metadata_server_L1.cpp authentication/authentication.cpp initiation/initiation.cpp

# Default target
all: $(METASERVER_TARGET)

# Build MetaServer
$(METASERVER_TARGET): $(METASERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean target
clean:
	rm -f $(METASERVER_TARGET) $(AUTHSERVER_TARGET) $(NOFYSERVER_TARGET)
