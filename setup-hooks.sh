#!/bin/bash

# Set up Git to use the custom hooks directory
git config core.hooksPath .githooks

echo "Git hooks have been set up successfully!"