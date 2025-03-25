g++ -std=c++20 -pthread -I/usr/include/x86_64-linux-gnu -L/usr/lib \
    dss_client.cpp \
    utils/Utils.cpp \
    Watcher/Watcher.cpp \
    Chunker/Chunker.cpp \
    Indexer/Indexer.cpp \
    Synchronization/APISynchronizer.cpp \
    main.cpp \
    -lcurl -lssl -lcrypto -linotify-cpp -o dss_client
./dss_client 