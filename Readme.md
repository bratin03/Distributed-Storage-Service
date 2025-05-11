# Distributed Storage Service

Distributed Storage Service is a distributed storage system that provides a simple and efficient way to store and retrieve files across multiple devices. It is designed to be easy to use and highly scalable, making it suitable for a wide range of applications. It currently supports only txt files. Similar to DropBox, it provides real-time synchronization and conflict resolution. 

# Usage

1. Clone the repository:
    ```bash
    git clone https://github.com/bratin03/Distributed-Storage-Service
    cd Distributed-Storage-Service
    ```
2. Install the required dependencies:
    ```bash
    ./requirements.sh
    ```
3. Run the server:
    ```bash
    ./server.sh
    ```
4. Run the client:
    ```bash
    ./client.sh [<username>] [<[password]>]
    ```

Provide a username and password to create a new user for the first time. If you want to use the user for which the last login was successful, you can run the client without any arguments.

This will create a directory called `Shared` in the current directory. You can create/modify/delete/rename files in this directory, and the changes will be synchronized across all devices running the client. The server will also keep track of the file versions and resolve any conflicts that may arise.

## Note
The current setup has been made to run all the clients and server on the same machine. To run different servers and clients on different machines, you need to change the IP address in the corresponding config files and at the load balancer.

For any queries, please feel free to reach out to us. [Contact Us](mailto:bratinmondal689@gmail.com)


## Contributors
- [@bratin03](https://github.com/bratin03) | [LinkedIn](https://in.linkedin.com/in/bratin-mondal-689b03m)
- [@Soukhin-Nayek](https://github.com/Soukhin-Nayek) | [LinkedIn](https://in.linkedin.com/in/soukhin-nayek-96b68b249)
- [@SwarnabhMondal](https://github.com/SwarnabhMondal) | [LinkedIn](https://in.linkedin.com/in/swarnabh-mandal-765962229)