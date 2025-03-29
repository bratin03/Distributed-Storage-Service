set -x

# Create the library directory
sudo mkdir -p /var/lib/DSS
sudo chown -R $USER:$USER /var/lib/DSS
sudo chmod -R 755 /var/lib/DSS
# Create the metadata database 
sudo mkdir -p /var/lib/DSS/metadata.db
sudo chown -R $USER:$USER /var/lib/DSS/metadata.db
sudo chmod -R 755 /var/lib/DSS/metadata.db
# Create the config directory
sudo mkdir -p /etc/DSS
sudo chown -R $USER:$USER /etc/DSS
sudo chmod -R 755 /etc/DSS
cp config.json /etc/DSS/config.json
