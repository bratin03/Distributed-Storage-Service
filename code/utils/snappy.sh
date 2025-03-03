# Installing snappy
echo "Installing snappy..."
cd snappy 
rm -rf build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. 
make -j$(nproc)
sudo make install
echo "Snappy installed successfully."

echo "Testing snappy installation..."
cd ..
./generate.sh 
./compile.sh
./snappy_example
echo "Snappy installation test completed."