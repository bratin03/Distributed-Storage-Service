# Improved Root-level Makefile
# ============================

# Directories producing static libraries.
LIB_SUBDIRS = cache fsUtils load_config login merge metadata serverUtils notification watcher kv process_local process_remote app_notify
# Boot is special since boot/boot.cpp is compiled in the client build.
BOOT_DIR = boot

# Compiler and linker configuration.
CXX      = g++
# Automatically construct include flags from directories. The dot (".") and boot directory
# are explicitly added because boot does not have its own Makefile.
INC_DIRS = . $(BOOT_DIR) $(LIB_SUBDIRS)
CXXFLAGS = -Wall -O2 -g -pg $(addprefix -I, $(INC_DIRS)) -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include $(shell pkg-config --cflags libnotify)
# Linker flags: look for libraries built in the subdirectories.
LDFLAGS  = $(addprefix -L, $(LIB_SUBDIRS))  
# Libraries produced in subdirectories plus extra libraries.
LIBS     = -lprocess_local -lprocess_remote -lfsUtils -lload_config -llogin -lmetadata -lserverUtils -lcache -lrocksdb -lkv -lapp_notify \
	 -lcrypto -lz -lpthread -lnotification -lwatcher -lcurl -lmerge -lgit2 $(shell pkg-config --libs libnotify)

# Default target.
all: client.out

# Build the client executable.
# It compiles client.cpp and boot/boot.cpp, linking against all subdirectory libraries.
client.out: client.cpp $(BOOT_DIR)/boot.cpp subdirs
	@echo "Building client.out..."
	$(CXX) $(CXXFLAGS) -o $@ client.cpp $(BOOT_DIR)/boot.cpp $(LDFLAGS) $(LIBS)

# Build subdirectories which contain their own Makefiles.
subdirs:
	@echo "Building libraries in subdirectories: $(LIB_SUBDIRS)"
	@for d in $(LIB_SUBDIRS); do \
	  $(MAKE) -C $$d || exit $$?; \
	done

# Clean target cleans the client executable and all libraries in subdirectories.
clean:
	@echo "Cleaning client.out and subdirectories..."
	@for d in $(LIB_SUBDIRS); do \
	  $(MAKE) -C $$d clean; \
	done
	$(RM) client.out

# Extra target: help prints available targets.
help:
	@echo "Available targets:"
	@echo "  all      - Build client.out and all subdirectories"
	@echo "  clean    - Clean client.out and all subdirectories"
	@echo "  help     - Show this help message"

.PHONY: all subdirs clean help
