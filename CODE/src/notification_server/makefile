# Makefile for building the notification server executable

# Compiler and flags.
CXX         = g++
CXXFLAGS    = -std=c++17 -Wall -I. -pthread -O2
LDFLAGS     = -lboost_system -lboost_thread -lcurl -lssl -lcrypto

# Source files for the server executable.
SERVER_SRCS = server.cpp notification_server.cpp http_listener.cpp
SERVER_OBJS = $(SERVER_SRCS:.cpp=.o)

.PHONY: all clean

# Default target builds only the server.
all: server.out

server.out: $(SERVER_OBJS)
	$(CXX) $(SERVER_OBJS) -o server.out $(LDFLAGS)

# Pattern rule for compiling .cpp files.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o server.out
