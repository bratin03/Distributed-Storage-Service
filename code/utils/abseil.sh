# Installing abseil
echo "Installing abseil..."
cd abseil
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
echo "Abseil installed successfully."

echo "Testing abseil installation..."
cd ..
./compile.sh
./hello_abseil
echo "Abseil installation test completed."