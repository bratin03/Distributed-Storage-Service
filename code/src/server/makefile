CXX = g++
CXXFLAGS = -std=c++17 -I../../utils/libraries/cpp-httplib -I../../utils/libraries/jwt-cpp/include -I/usr/include/nlohmann -I./logger
LDFLAGS = -L/usr/lib -lssl -lcrypto -lpthread

# Targets
METASERVER_TARGET = metaserver
AUTHSERVER_TARGET = auth_server
NOFYSERVER_TARGET = notify_server

# Source files
METASERVER_SRC = metadata_server_L1.cpp
AUTHSERVER_SRC = authentication_server.cpp
NOFYSERVER_SRC = notification_server.cpp



# Compile MetaServer
$(METASERVER_TARGET): $(METASERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(METASERVER_TARGET) $(METASERVER_SRC) $(LDFLAGS)

# Compile AuthServer
$(AUTHSERVER_TARGET): $(AUTHSERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(AUTHSERVER_TARGET) $(AUTHSERVER_SRC) $(LDFLAGS)

# Compile Notification
$(NOFYSERVER_TARGET): $(NOFYSERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $(NOFYSERVER_TARGET) $(NOFYSERVER_SRC) $(LDFLAGS)

# Clean all compiled files
clean:
	rm -f $(METASERVER_TARGET) $(AUTHSERVER_TARGET)
