echo -e "\e[31mRun as superuser - 'sudo su'\e[0m"

set -x
sudo apt-get update
sudo apt-get install libhiredis-dev -y

../redis/create_redis_instance.sh 127.0.0.2 10000
../redis/start_redis_instance.sh 127.0.0.2 10000

cd hiredis
./compile.sh

./main.out

cd ..
../redis/stop_delete_redis_instance.sh 127.0.0.2 10000
