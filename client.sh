cd CODE/src/client
make
./db_setup.sh
./dir_setup.sh

# Check if two arguments are passed
if [ "$#" -ne 2 ]; then
    echo "Username and password are not provided."
    echo "Using default username and password."
    ./client.out
else
    # Assign arguments to variables
    USERNAME=$1
    PASSWORD=$2

    # Check if the username and password are not empty
    if [ -z "$USERNAME" ] || [ -z "$PASSWORD" ]; then
        echo "Username and password cannot be empty."
        echo "Using default username and password."
        ./client.out
    else
        # Run the client with the provided username and password
        ./client.out "$USERNAME" "$PASSWORD"
    fi
fi
