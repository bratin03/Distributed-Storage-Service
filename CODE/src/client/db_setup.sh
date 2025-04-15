sudo rm -rf /var/lib/DSS

sudo mkdir -p /var/lib/DSS
sudo chown -R $USER:$USER /var/lib/DSS
sudo chmod -R 755 /var/lib/DSS

sudo mkdir -p /var/lib/DSS/metadata
sudo chown -R $USER:$USER /var/lib/DSS/metadata
sudo chmod -R 755 /var/lib/DSS/metadata