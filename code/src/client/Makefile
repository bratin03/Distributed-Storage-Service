# Root-level Makefile
# ===================

# Define subdirectories that contain their own make targets.
# These directories are expected to compile into static libraries.
SUBDIRS = cache fsUtils load_config login merge metadata boot serverUtils notification

# Compiler and linker flags
CXX      = g++
CXXFLAGS = -Wall -O2 -g -pg\
           -I. -Iboot -Icache -IfsUtils -Iload_config -Ilogin -Imerge -Imetadata -IserverUtils -Inotification
# Linker flags to look for libraries built in subdirectories.
LDFLAGS = -Lcache -LfsUtils -Lload_config -Llogin -Lmerge -Lmetadata -LserverUtils -Lnotification
# List of static libraries produced in the subdirectories.
LIBS    = -lfsUtils -lload_config -llogin -lmerge -lmetadata -lserverUtils -lcache -lrocksdb -lcrypto -lz -lpthread -lnotification

# Default target; build subdirectories and then build the client executable.
all: client.out

# Build the client executable.
# Note: client.cpp (top-level source) and boot/boot.cpp (compiled here because no Makefile is in boot/)
client.out: client.cpp boot/boot.cpp | subdirs
	$(CXX) $(CXXFLAGS) -o $@ client.cpp boot/boot.cpp $(LDFLAGS) $(LIBS)

# Rule to descend into each subdirectory and build the corresponding targets.
subdirs:
	@for d in $(SUBDIRS); do \
		$(MAKE) -C $$d || exit 1; \
	done

# Clean: clean the client and then clean in all subdirectories.
clean:
	@for d in $(SUBDIRS); do \
		$(MAKE) -C $$d clean; \
	done
	rm -f client.out

.PHONY: all clean subdirs
