
# directory/file deletion 
user send delete request to metadata server 
metadata server check for existing file and sub directory 
if exist 
    respond with ( ip, port) of file , directory 
else 
    set status to delete at the metadata
    send a request to the notification server 
    respond client with success 

notification server send notification 
After sending send a request to /commitDelete in metadata server

metadata server delete the file metadata, chunk metadata, directory metadata and modify the parent directory metadata 



creation 
client 
    -> load balancer 
    -> metadata server - ( creating metadata ) 
    -> notification server 

update 
client 
    -> load balancer 
    -> metadata server - ( updating version and returning block server list ) 
    -> blockserver ( checks version, send confirm/reject to user ) after finishing sending confirmation to metaserver 
    -> load balancer . metaserver 
    -> load balancer . notification server 

deletion 
client 
    -> load balancer 
    -> metadata server 
    -> load balancer . notification server 
    -> notification server ( sending confirmation to metadata server )
    -> metaserver sending confirmation to client and sending signal to delete in block server 
    -> block server deletes the file 
