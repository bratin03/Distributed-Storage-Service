Building
========

You need the CMake version specified in [CMakeLists.txt](./CMakeLists.txt)
or later to build:

```bash
git submodule update --init
mkdir build
cd build && cmake ../ && make
```

Usage
=====

To use Snappy from your own C++ program, include the file "snappy.h" from
your calling file, and link against the compiled library.

There are many ways to call Snappy, but the simplest possible is

```c++
snappy::Compress(input.data(), input.size(), &output);
```
and similarly

```c++
snappy::Uncompress(input.data(), input.size(), &output);
```

where "input" and "output" are both instances of std::string.

To compile:
```bash
g++ -std=c++11 main.cpp -o snappy_example -lsnappy
```