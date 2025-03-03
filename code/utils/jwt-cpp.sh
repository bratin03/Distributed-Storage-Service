# Installing abseil
echo "Installing jwt-cpp..."
cd jwt-cpp
rm -rf build
mkdir build && cd build
cmake .. -DJWT_BUILD_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=Release
sudo make install
echo "jwt-cpp installed successfully."

echo "Testing jwt-cpp installation..."
cd ..
./compile.sh
./jwt_example
echo "jwt-cpp installation test completed."