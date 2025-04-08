#!/bin/bash

# Script to generate RSA key pair

# Output filenames
PRIVATE_KEY="private.pem"
PUBLIC_KEY="public.pem"

echo "Generating 2048-bit RSA private key..."
openssl genpkey -algorithm RSA -out "$PRIVATE_KEY" -pkeyopt rsa_keygen_bits:2048

if [[ $? -ne 0 ]]; then
    echo "❌ Failed to generate private key."
    exit 1
fi

echo "Extracting public key from private key..."
openssl rsa -in "$PRIVATE_KEY" -pubout -out "$PUBLIC_KEY"

if [[ $? -ne 0 ]]; then
    echo "❌ Failed to extract public key."
    exit 1
fi

echo "✅ RSA key pair generated successfully:"
echo "  - Private Key: $PRIVATE_KEY"
echo "  - Public Key : $PUBLIC_KEY"
