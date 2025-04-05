# Compiler and flags
CXX       = g++
CXXFLAGS  = -std=c++17 -O2 -Wall -Wextra -g -pg
LIBS      = -lboost_system -lpthread

# Targets
all: server client

server: main.o notification_server.o
	$(CXX) $(CXXFLAGS) -o server main.o notification_server.o $(LIBS)

client: client.o notification_client.o
	$(CXX) $(CXXFLAGS) -o client client.o notification_client.o $(LIBS)

# Compile source files
main.o: main.cpp notification_server.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

notification_server.o: notification_server.cpp notification_server.hpp
	$(CXX) $(CXXFLAGS) -c notification_server.cpp

notification_client.o: notification_client.cpp notification_client.hpp
	$(CXX) $(CXXFLAGS) -c notification_client.cpp

client.o: client.cpp notification_client.cpp
	$(CXX) $(CXXFLAGS) -c client.cpp notification_client.cpp

clean:
	rm -f *.o server client
