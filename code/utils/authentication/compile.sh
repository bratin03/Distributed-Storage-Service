g++ -std=c++17 -O2 login_server.cpp -o login_server.out -lboost_system -lssl -lcrypto -ljwt
g++ -std=c++17 -O2 client.cpp -o client.out -lboost_system -lssl -lcrypto -ljwt
g++ -std=c++17 -O2 other_server.cpp -o other_server.out -lboost_system -lssl -lcrypto -ljwt
