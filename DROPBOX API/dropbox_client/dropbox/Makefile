# Compiler and Archiver settings
CXX      = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -c -O2
AR       = ar
ARFLAGS  = rcs
LIB      = libdropbox.a

# Source files and corresponding object files
SRCS = bootup_1.cpp bootup_2.cpp bootup_3.cpp dropbox_client.cpp
OBJS = $(SRCS:.cpp=.o)

# Default target
all: $(LIB)

# Build the static library from object files
$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

# Compile each source file into an object file
%.o: %.cpp dropbox_client.h
	$(CXX) $(CXXFLAGS) $< -o $@

# Clean up generated files
clean:
	rm -f $(OBJS) $(LIB)

.PHONY: all clean
