echo "Installing libjwt"
sudo apt update
sudo apt install libjwt-dev libssl-dev -y
echo "libjwt installed"

echo "Testing libjwt"
cd libjwt
./compile.sh
./jwt_example
echo "libjwt tested"