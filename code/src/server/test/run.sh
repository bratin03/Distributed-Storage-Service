g++ -std=c++11 -pthread client1.cpp -o client1
g++ -std=c++11 -pthread client2.cpp -o client2

./client1 127.0.0.1:8002 1