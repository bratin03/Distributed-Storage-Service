# Improved Root-level Makefile with Explicit Declarations
# ================================================

# Compiler and linker configuration
CXX      := g++
CXXFLAGS := -Wall -O2 -g -pg \
            -I. -Iboot -Icache -IfsUtils -Iload_config -Ilogin -Imerge \
            -Imetadata -IserverUtils -Inotification -Iwatcher -Ikv \
            -Iprocess_local -Iprocess_remote -Iapp_notify \
            -I/usr/include/glib-2.0 \
            -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
            $(shell pkg-config --cflags libnotify) \
            -std=c++17
LDFLAGS  := -Lcache -LfsUtils -Lload_config -Llogin -Lmerge \
            -Lmetadata -LserverUtils -Lnotification -Lwatcher \
            -Lkv -Lprocess_local -Lprocess_remote -Lapp_notify
LIBS     := -lprocess_local -lprocess_remote -lfsUtils -lload_config \
            -llogin -lmetadata -lserverUtils -lcache -lrocksdb -lkv \
            -lapp_notify -lcrypto -lz -lpthread -lnotification \
            -lwatcher -lcurl -lmerge -lgit2 $(shell pkg-config --libs libnotify)

# Default target
all: client.out

# Link client executable
client.out: client.cpp boot/boot.cpp cache/libcache.a fsUtils/libfsUtils.a \
             load_config/libload_config.a login/liblogin.a merge/libmerge.a \
             metadata/libmetadata.a serverUtils/libserverUtils.a \
             notification/libnotification.a watcher/libwatcher.a \
             kv/libkv.a process_local/libprocess_local.a \
             process_remote/libprocess_remote.a app_notify/libapp_notify.a
	@echo "Building client.out..."
	$(CXX) $(CXXFLAGS) -o $@ client.cpp boot/boot.cpp $(LDFLAGS) $(LIBS)

# Explicit library build rules
cache/libcache.a:
	@echo "Building cache library..."
	$(MAKE) -C cache

fsUtils/libfsUtils.a:
	@echo "Building fsUtils library..."
	$(MAKE) -C fsUtils

load_config/libload_config.a:
	@echo "Building load_config library..."
	$(MAKE) -C load_config

login/liblogin.a:
	@echo "Building login library..."
	$(MAKE) -C login

merge/libmerge.a:
	@echo "Building merge library..."
	$(MAKE) -C merge

metadata/libmetadata.a:
	@echo "Building metadata library..."
	$(MAKE) -C metadata

serverUtils/libserverUtils.a:
	@echo "Building serverUtils library..."
	$(MAKE) -C serverUtils

notification/libnotification.a:
	@echo "Building notification library..."
	$(MAKE) -C notification

watcher/libwatcher.a:
	@echo "Building watcher library..."
	$(MAKE) -C watcher

kv/libkv.a:
	@echo "Building kv library..."
	$(MAKE) -C kv

process_local/libprocess_local.a:
	@echo "Building process_local library..."
	$(MAKE) -C process_local

process_remote/libprocess_remote.a:
	@echo "Building process_remote library..."
	$(MAKE) -C process_remote

app_notify/libapp_notify.a:
	@echo "Building app_notify library..."
	$(MAKE) -C app_notify

# Clean all artifacts
clean:
	@echo "Cleaning client.out and libraries..."
	$(RM) client.out
	$(MAKE) -C cache clean
	$(MAKE) -C fsUtils clean
	$(MAKE) -C load_config clean
	$(MAKE) -C login clean
	$(MAKE) -C merge clean
	$(MAKE) -C metadata clean
	$(MAKE) -C serverUtils clean
	$(MAKE) -C notification clean
	$(MAKE) -C watcher clean
	$(MAKE) -C kv clean
	$(MAKE) -C process_local clean
	$(MAKE) -C process_remote clean
	$(MAKE) -C app_notify clean

# Help message
help:
	@echo "Available targets:"
	@echo "  all      - Build client.out and all libraries"
	@echo "  clean    - Clean client.out and all libraries"
	@echo "  help     - Show this help message"

.PHONY: all clean help
