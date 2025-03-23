#!/bin/bash
# This script installs the OpenSSL development libraries on Ubuntu.

echo "Updating package list..."
sudo apt-get update

echo "Installing libssl-dev..."
sudo apt-get install -y libssl-dev

echo "Installation complete. Remember to compile your code with -lcrypto."
