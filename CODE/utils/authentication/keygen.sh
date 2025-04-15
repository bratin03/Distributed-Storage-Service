#!/bin/bash

# Create ssl_keys directory (if it doesn't already exist)
mkdir -p ssl_keys
cd ssl_keys

# Generate a 2048-bit private key
openssl genrsa -out server.key 2048

# Create a certificate signing request (CSR) non-interactively using your details
openssl req -new -key server.key -out server.csr \
  -subj "/C=IN/ST=West Bengal/L=Kharagpur/O=Indian Institute of Technology Kharagpur/OU=Computer Science and Engineering/CN=CSE IIT KGP/emailAddress=bratinmondal689@gmail.com"

# Self-sign the CSR to create a certificate valid for 365 days
openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

# Create login_keys directory (if it doesn't already exist)
mkdir -p ../login_keys
cd ../login_keys

# Generate a 2048-bit RSA private key
openssl genrsa -out private.pem 2048

# Extract the public key
openssl rsa -in private.pem -pubout -out public.pem
