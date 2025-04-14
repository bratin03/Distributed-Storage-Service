CXX = g++
CXXFLAGS = -std=c++11 -Wall -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include $(shell pkg-config --cflags libnotify)
LDLIBS = $(shell pkg-config --libs libnotify)

TARGET = notification_app.out
STATICLIB = libapp_notify.a

OBJS = main.o app_notify.o

.PHONY: all clean

all: $(STATICLIB) $(TARGET)

# Create the static library from app_notify.o
$(STATICLIB): app_notify.o
	@echo "Creating static library $(STATICLIB)..."
	ar rcs $(STATICLIB) app_notify.o

# Link the executable with main.o and the static library
$(TARGET): main.o $(STATICLIB)
	$(CXX) -o $(TARGET) main.o $(STATICLIB) $(LDLIBS)

# Compile main.cpp
main.o: main.cpp app_notify.hpp
	$(CXX) $(CXXFLAGS) -c main.cpp

# Compile app_notify.cpp
app_notify.o: app_notify.cpp app_notify.hpp
	$(CXX) $(CXXFLAGS) -c app_notify.cpp

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(TARGET) $(STATICLIB)
