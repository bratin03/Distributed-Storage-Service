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

# Check if the OS is macOS (Darwin)
if [ "$(uname)" = "Darwin" ]; then
    ./generate_mac.sh
else
    ./generate.sh
fi

./compile.sh
./snappy_example
echo "Snappy installation test completed."
