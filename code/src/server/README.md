
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