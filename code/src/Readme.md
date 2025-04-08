
signup server 127.0.0.1:10000
./compile.sh
foreman start 
./start_nginx.sh

password_db  127.0.0.1:10010
    ./create.sh
    ./start.sh

meta_blockserver 
foreman start 

authentication_server 127.0.0.1:30000
make 

metaserver 127.0.0.3:30000



utils/redis/ 
    ./create_redis_instance.sh 
    ./start_redis_instance.sh


work -> 
    load balancer in metaserver 
    load balancer in authentication server 
    notification server intigration 
    garbage collection thread
    update testing 
