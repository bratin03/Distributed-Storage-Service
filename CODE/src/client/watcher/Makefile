CXX = g++
CXXFLAGS = -Wall -Wextra -g -pg -O2 -std=c++17
AR = ar
ARFLAGS = rcs

TARGET = libwatcher.a
OBJ = watcher.o
HDR = watcher.hpp

all: $(TARGET)

$(TARGET): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

watcher.o: watcher.cpp $(HDR)
	$(CXX) $(CXXFLAGS) -c watcher.cpp -o watcher.o

clean:
	rm -f $(TARGET) $(OBJ)
