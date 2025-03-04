# Installing dependencies
echo "Installing required packages..."
sudo apt-get update
sudo apt-get install -y \
  libboost-all-dev \
  libdouble-conversion-dev \
  libevent-dev \
  libgoogle-glog-dev \
  libgflags-dev \
  libiberty-dev \
  liblz4-dev \
  liblzma-dev \
  libsnappy-dev \
  make \
  cmake \
  g++ \
  pkg-config \
  libfmt-dev
echo "Required packages installed."

# Installing fmt
echo "Installing fmt..."
cd fmt
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
cd ../..
echo "fmt installed successfully."

# Installing folly
echo "Installing folly..."
cd folly
rm -rf _build
mkdir _build && cd _build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
cd ..
echo "Folly installed successfully."

# Testing folly installation
echo "Testing folly installation..."
./compile.sh
./fbstring_example.out
if [ $? -eq 0 ]; then
    echo "Folly installation test passed."
else
    echo "Folly installation test failed."
fi