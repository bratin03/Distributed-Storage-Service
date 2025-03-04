echo "Installing Boost..."
sudo apt update
sudo apt install libboost-all-dev libssl-dev -y
echo "Boost installation completed."

echo "Testing Boost installation..."
cd boost
./compile.sh
./ssl_client.out
if [ $? -eq 0 ]; then
    echo "Boost installation test passed."
else
    echo "Boost installation test failed."
fi