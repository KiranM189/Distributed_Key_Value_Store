cmake_minimum_required(VERSION 3.10)

project(KVServerProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Find required packages using Spack paths
find_package(PkgConfig REQUIRED)

# Include paths and libraries for the Mochi libraries
pkg_check_modules(MARGO REQUIRED IMPORTED_TARGET margo)
pkg_check_modules(ABT REQUIRED IMPORTED_TARGET argobots)
pkg_check_modules(MERCURY REQUIRED IMPORTED_TARGET mercury)

# Find Thallium (if it provides a CMake package)
find_package(thallium REQUIRED)

# Include directories
include_directories(include)

# Add server executable
add_executable(kvm_server src/KVServer.cpp src/main_server.cpp)

# Link the libraries (Margo, Argobots, Mercury, Thallium)
target_link_libraries(kvm_server
    PkgConfig::MARGO
    PkgConfig::ABT
    PkgConfig::MERCURY
    thallium
)

target_include_directories(kvm_server PRIVATE
    ${MARGO_INCLUDE_DIRS}
    ${ABT_INCLUDE_DIRS}
    ${MERCURY_INCLUDE_DIRS}
    ${THALLIUM_INCLUDE_DIRS}
)

# Add client executable
add_executable(kvm_client src/KVClient.cpp src/main_client.cpp)

target_link_libraries(kvm_client
    PkgConfig::MARGO
    PkgConfig::ABT
    PkgConfig::MERCURY
    thallium
)

target_include_directories(kvm_client PRIVATE
    ${MARGO_INCLUDE_DIRS}
    ${ABT_INCLUDE_DIRS}
    ${MERCURY_INCLUDE_DIRS}
    ${THALLIUM_INCLUDE_DIRS}
)