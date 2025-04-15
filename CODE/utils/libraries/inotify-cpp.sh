set -x
sudo apt-get update
sudo apt-get install build-essential cmake libboost-all-dev

cd inotify-cpp
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
cmake --build .
# (Optional) run tests:
ctest -VV
# Install the library (you may need sudo):
sudo cmake --build . --target install
