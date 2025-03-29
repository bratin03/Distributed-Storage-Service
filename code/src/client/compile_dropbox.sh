set -x
g++ dropbox_main.cpp -L./dropbox -ldropbox_client -L./metadata_dropbox -lmetadata -O2 -o client.out -lrocksdb -lcurl -lgit2