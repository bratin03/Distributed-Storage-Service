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
SRCS = metadata_server_L1.cpp authentication/authentication.cpp initiation/initiation.cpp database_handler/database_handler.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(METASERVER_TARGET)

# Link object files into executable
$(METASERVER_TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files into object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(METASERVER_TARGET) $(OBJS)
