# Makefile

CXX = g++
CXXFLAGS = -std=c++17 -O2 -pthread
LDFLAGS = -lhiredis -lcurl

TARGET = signup_server.out
SRC = signup_server.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
