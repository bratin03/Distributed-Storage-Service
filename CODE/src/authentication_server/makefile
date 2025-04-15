CXX = g++
CXXFLAGS = -std=c++17 -I../../utils/libraries/cpp-httplib -I../../utils/libraries/jwt-cpp/include -I/usr/include/nlohmann -I./logger -O2
LDFLAGS = -L/usr/lib -lssl -lcrypto -pthread -lhiredis -lcurl

# Targets
METASERVER_TARGET = metaserver.out
AUTHSERVER_TARGET = auth_server.out
NOFYSERVER_TARGET = notify_server.out

# Source files
METASERVER_SRC = metadata_server_L1.cpp
AUTHSERVER_SRC = authentication_server.cpp
NOFYSERVER_SRC = notification_server.cpp


# Compile AuthServer
$(AUTHSERVER_TARGET): $(AUTHSERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(AUTHSERVER_TARGET) $(AUTHSERVER_SRC) $(LDFLAGS)

# Clean all compiled files
clean:
	rm -f $(AUTHSERVER_TARGET)
