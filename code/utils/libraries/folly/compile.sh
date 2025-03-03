g++ -std=c++17 main.cpp -L/usr/local/lib -lfolly -Wl,--whole-archive -lfmt -Wl,--no-whole-archive -lglog -levent -pthread -o fbstring_example
