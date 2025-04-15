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

./compile.sh
./snappy_example.out
if [ $? -eq 0 ]; then
    echo "Snappy installation test passed."
else
    echo "Snappy installation test failed."
fi