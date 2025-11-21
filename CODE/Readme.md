# Distributed Storage Service Codebase

This directory contains the source code for the Distributed Storage Service.

## Build Instructions

The project uses CMake for building.

### Prerequisites
- CMake 3.10+
- C++23 compatible compiler (GCC 13+)
- OpenSSL
- libcurl
- libnotify
- glib-2.0
- rocksdb
- hiredis
- libgit2

### Building

#### Quick Start (Release Build)
1. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```
2. Configure the project:
   ```bash
   cmake ..
   ```
3. Build:
   ```bash
   make -j$(nproc)
   ```

#### Build Types

The project supports multiple build types for different use cases:

**Debug Build** - For development and debugging:
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```
- No optimization (`-O0`)
- Full debug symbols (`-g`)
- Best for development

**Release Build** - For production (default):
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```
- Maximum optimization (`-O3`)
- No debug symbols
- Asserts disabled

**RelWithDebInfo** - For testing with debug capability:
```bash
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make -j$(nproc)
```
- Moderate optimization (`-O2`), debug symbols (`-g`)

**Profile** - For performance analysis:
```bash
cmake -DCMAKE_BUILD_TYPE=Profile ..
make -j$(nproc)
```
- Optimization (`-O2`), profiling enabled (`-pg`)

> **Note**: When switching build types, clean first: `rm -rf build/*` or `make clean`

## Executables
After a successful build, the following executables will be available in `CODE/build/src/`:

- `client/client`: The client application.
- `signupserver/signup_server`: The signup server.
- `metaserver/metaserver`: The metadata server.
- `authentication_server/auth_server`: The authentication server.
- `notification_server/notify_server`: The notification server.
- `data_blockserver/data_block_client`: Test client for data blockserver.
- `meta_blockserver/meta_block_client`: Test client for meta blockserver.

## Running
Ensure you have the necessary configuration files in `config/` directories relative to the executables or as specified in the usage instructions.

### Client
```bash
./src/client/client
```

### Servers
- **Signup Server**: `./src/signupserver/signup_server`
- **Metadata Server**: `./src/metaserver/metaserver <config_path>`
- **Authentication Server**: `./src/authentication_server/auth_server <config_path>`
- **Notification Server**: `./src/notification_server/notify_server <config_path>`

### Python Servers
The following servers are Python-based and do not require compilation (except for their test clients):
- **Data Blockserver**: Located in `src/data_blockserver/`. Run with `python3 server.py`.
- **Meta Blockserver**: Located in `src/meta_blockserver/`. Run with `python3 server.py`.

### Scripts
- **Password DB**: Scripts located in `src/password_db/` to manage the Redis password database.

