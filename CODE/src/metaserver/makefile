CXX = g++
CXXFLAGS = -std=c++17 \
	-I../../utils/libraries/cpp-httplib \
	-I../../utils/libraries/jwt-cpp/include \
	-I/usr/include/nlohmann \
	-I./logger \
	-Wall -Wextra -g -O2

LDFLAGS = -L/usr/lib -lssl -lcrypto -lpthread -lhiredis -lcurl -pg

# Targets
METASERVER_TARGET = metaserver.out

# Source files
SRCS = metadata_server_L1.cpp \
       authentication/authentication.cpp \
       initiation/initiation.cpp \
       database_handler/database_handler.cpp \
       deletion_manager/deletion_manager.cpp

# Object files
OBJS = metadata_server_L1.o \
       authentication/authentication.o \
       initiation/initiation.o \
       database_handler/database_handler.o \
       deletion_manager/deletion_manager.o

# Default target
all: $(METASERVER_TARGET)

# Explicit compilation steps
metadata_server_L1.o: metadata_server_L1.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

authentication/authentication.o: authentication/authentication.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

initiation/initiation.o: initiation/initiation.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

database_handler/database_handler.o: database_handler/database_handler.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

deletion_manager/deletion_manager.o: deletion_manager/deletion_manager.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link object files into final executable
$(METASERVER_TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean target
clean:
	rm -f $(METASERVER_TARGET) $(OBJS)
