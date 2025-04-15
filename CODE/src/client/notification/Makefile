# Makefile

CXX = g++
CXXFLAGS = -O2 -g -pg -Wall -std=c++17
AR = ar
ARFLAGS = rcs
TARGET = libnotification.a
SRC = notification.cpp
OBJ = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(AR) $(ARFLAGS) $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
