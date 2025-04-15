echo "Installing libjwt"
sudo apt update
sudo apt install libjwt-dev libssl-dev -y
echo "libjwt installed"

echo "Testing libjwt"
cd libjwt
./compile.sh
./jwt_example.out
if [ $? -eq 0 ]; then
    echo "libjwt installation test passed."
else
    echo "libjwt installation test failed."
fi