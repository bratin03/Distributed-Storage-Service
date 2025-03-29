#!/bin/bash
set -x

g++ -c dropbox_client.cpp -lcurl -pthread -O2 -std=c++17 -lmetadata -L./metadata_dropbox -g -pg
g++ -c bootup_1.cpp -lcurl -pthread -O2 -std=c++17 -lmetadata -L./metadata_dropbox -g -pg

ar rcs libdropbox_client.a dropbox_client.o bootup_1.o
    