# Distributed_Key_Value_Store
To design, implement and evaluate a distributed, scalable Key-Value store using a map on Shared memory and enabling access to the same across all nodes on the cluster using an RPC/RDMA framework.

## Project Overview
- This project focuses on dynamically adding or removing nodes and storing a large number of key-value pairs using a map on shared memory.
- Key-value pairs are distributed across various nodes using key % number_of_nodes logic.
- Key is set to integer datatype, while value is set to string datatype.
- Redistribution and load-balancing of key-value pairs takes place when new nodes are added or removed.
- Provides both local and remote fetch for all key-value pairs along with other various functionalities

## Features
- PUT key-value pair
- GET key-value pair
- UPDATE key-value pair
- DELETE key-value pair
- Add node to the cluster
- Remove node from the cluster
- List all nodes
- Show distribution of keys across various nodes

## Required Dependencies
- CMake (version 3.10 or higher)
- C++ Compiler with C++14 support (GCC/Clang)
- Spack (for managing HPC dependencies)
- Boost libraries
- Mochi libraries: Margo, Argobots, Mercury, Thallium

## Install Dependencies with Spack
- To install spack
  git clone https://github.com/spack/spack.git
  export PATH=$PATH:$(pwd)/spack/bin

- To install the dependencies
  spack install margo
  spack install argobots
  spack install mercury
  spack install thallium
  spack install boost

- To load spack environment
  spack load margo argobots mercury thallium boost

## Build Instructions
- Clone the repo and go to the integration branch

mkdir build
cd build

- Configure with CMake
cmake ..

- Build the project
make

- Running the system (starting the server)
Default:
./start_nodes.sh

Custom memory allocation:
./start_nodes.sh -m 1G

Custom build directory
./start_nodes.sh -d /path/to/build -m 500M

Same above given run instructions can be run on multiple machines to start the server.

- Starting the client
Default (will prompt to add nodes)
./kvm_client_integrated

With predefined nodes
./kvm_client_integrated --node ofi+tcp://192.168.1.100:8080 --node ofi+tcp://192.168.1.101:8080

- Stopping the server
kill <SERVER_PID>
