g++ -std=c++17 -pthread -I. dss_client.cpp main.cpp -lcurl -lssl -lcrypto -o dss_client

./dss_client /root_dss https://dss.example.com/api