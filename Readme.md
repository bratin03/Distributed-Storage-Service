# Distributed Storage Service

A distributed storage system providing efficient file storage, real-time synchronization, and conflict resolution across multiple devices. Designed for scalability and ease of use, it supports text file synchronization similar to Dropbox.

## Features
- **Real-time Synchronization**: Changes are propagated instantly across connected clients.
- **Conflict Resolution**: Automatically handles file versioning and conflicts.
- **Distributed Architecture**: Scalable design with separate metadata and block servers.
- **Secure**: User authentication and secure communication.

## Components
- **Client**: User interface for file synchronization.
- **Metadata Server**: Manages file metadata and directory structure.
- **Data Blockserver**: Stores actual file content in blocks.
- **Meta Blockserver**: Manages metadata for blocks.
- **Authentication Server**: Handles user login and JWT issuance.
- **Notification Server**: Pushes real-time updates to clients.
- **Signup Server**: Handles new user registration.
- **Password DB**: Redis-backed storage for user credentials.

## Getting Started

### Prerequisites
- Linux environment
- C++23 compatible compiler
- CMake 3.10+
- Dependencies: OpenSSL, libcurl, libnotify, glib-2.0, rocksdb, hiredis, libgit2

### Installation

1. **Clone the repository:**
    ```bash
    git clone https://github.com/bratin03/Distributed-Storage-Service
    cd Distributed-Storage-Service
    ```

2. **Install dependencies:**
    ```bash
    ./requirements.sh
    ```

3. **Build the project:**
    ```bash
    cd CODE
    mkdir build && cd build
    cmake ..                          # Release build (default)
    # OR
    cmake -DCMAKE_BUILD_TYPE=Debug .. # Debug build
    make -j$(nproc)
    ```
    
    See [CODE/Readme.md](CODE/Readme.md) for detailed build instructions and available build types (Debug, Release, RelWithDebInfo, Profile).

## Development

### Coding Guidelines
This project follows industry-standard coding practices. See [CODING_GUIDELINES.md](CODING_GUIDELINES.md) for detailed standards on:
- Naming conventions (PascalCase, snake_case, camelCase)
- Code structure and organization
- Documentation requirements
- Modern C++ best practices

### Formatting
Use the provided scripts to format your code before committing:
```bash
./format_cpp.sh      # Format C++ files
./format_python.sh   # Format Python files
```


### Usage

1. **Start the Server:**
    ```bash
    ./server.sh
    ```

2. **Start the Client:**
    ```bash
    ./client.sh [<username>] [<password>]
    ```
    - First time login: Provide username and password to register.
    - Subsequent logins: Run without arguments to use cached credentials.

    A `Shared` directory will be created. Files in this directory are synchronized.

## Configuration
The system is configured to run locally by default. To deploy on multiple machines, update the IP addresses in the configuration files located in `CODE/config/` and the load balancer settings.

## Contributors
- [@bratin03](https://github.com/bratin03) | [LinkedIn](https://in.linkedin.com/in/bratin-mondal-689b03m)
- [@Soukhin-Nayek](https://github.com/Soukhin-Nayek) | [LinkedIn](https://in.linkedin.com/in/soukhin-nayek-96b68b249)
- [@SwarnabhMondal](https://github.com/SwarnabhMondal) | [LinkedIn](https://in.linkedin.com/in/swarnabh-mandal-765962229)

## Contact
For queries, please [Contact Us](mailto:bratinmondal689@gmail.com).