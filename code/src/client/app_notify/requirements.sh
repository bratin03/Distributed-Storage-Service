sudo apt-get update -y
sudo apt-get install libnotify-dev libglib2.0-dev pkg-config -y
sudo apt-get install libgdk-pixbuf2.0-dev shared-mime-info -y

export PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig:$PKG_CONFIG_PATH