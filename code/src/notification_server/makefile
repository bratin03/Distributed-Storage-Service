# Makefile for building the notification client and server executables

# Compiler and flags.
CXX         = g++
CXXFLAGS    = -std=c++17 -Wall -I. -pthread -O2
LDFLAGS     = -lboost_system -lboost_thread

# Source files for the client executable.
CLIENT_SRCS = client.cpp notification_client.cpp 
CLIENT_OBJS = $(CLIENT_SRCS:.cpp=.o)

# Source files for the server executable.
SERVER_SRCS = server.cpp notification_server.cpp http_listener.cpp
SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)

.PHONY: all clean

# Default target builds both client and server.
all: client.out server.out

client.out: $(CLIENT_OBJS)
	$(CXX) $(CLIENT_OBJS) -o client.out $(LDFLAGS)

server.out: $(SERVER_OBJS)
	$(CXX) $(SERVER_OBJS) -o server.out $(LDFLAGS)

# Pattern rule for compiling .cpp files.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o client.out server.out
