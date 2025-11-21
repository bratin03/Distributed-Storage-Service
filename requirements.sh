#!/bin/bash
echo "=== System Update ==="
sudo apt update -y && sudo apt upgrade -y

echo "=== Installing System Dependencies ==="
sudo apt install -y \
    nlohmann-json3-dev \
    libhiredis-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    nginx \
    redis-server \
    ruby-foreman \
    libboost-all-dev \
    librocksdb-dev \
    libgit2-dev \
    libnotify-dev \
    libglib2.0-dev \
    pkg-config \
    libgdk-pixbuf2.0-dev \
    shared-mime-info \
    xterm \
    cmake \
    python3-pip \
    python3-flask \
    python3-requests \
    python3-venv \
    clang-format

echo "=== Configuring Environment Variables ==="
PKG_CONFIG_LINE='export PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:$PKG_CONFIG_PATH'
BASHRC="$HOME/.bashrc"
APPENDED=0

if ! grep -Fxq "$PKG_CONFIG_LINE" "$BASHRC"; then
    echo "Appending PKG_CONFIG_PATH to .bashrc..."
    # Ensure newline before appending if needed
    if [ -s "$BASHRC" ] && [ "$(tail -c1 "$BASHRC")" != "" ]; then
        echo >> "$BASHRC"
    fi
    echo "$PKG_CONFIG_LINE" >> "$BASHRC"
    APPENDED=1
else
    echo "PKG_CONFIG_PATH is already present in .bashrc."
fi

if [ "$APPENDED" -eq 1 ]; then
    echo "Sourcing updated .bashrc..."
    source "$BASHRC"
fi

echo "=== Checking OpenSSL Version ==="
OPENSSL_VERSION=$(openssl version | awk '{print $2}')
OPENSSL_MAJOR_VERSION=$(echo "$OPENSSL_VERSION" | cut -d. -f1)

echo "Detected OpenSSL version: $OPENSSL_VERSION"

if [ "$OPENSSL_MAJOR_VERSION" -lt 3 ]; then
    echo "OpenSSL version is less than 3 — running openssl.sh..."
    if [ -x "./openssl.sh" ]; then
        ./openssl.sh
    else
        echo "Error: openssl.sh not found or not executable."
        exit 1
    fi
else
    echo "OpenSSL version is $OPENSSL_VERSION — no action needed."
fi

echo "=== Setup Complete ==="