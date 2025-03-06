# nxsan
*Thank you, nxsan! Our memory is secure another day!*

## Requirements
On Linux, an installed copy of ZLib is required for configuring LLVM.

## Building
Before building, clone all submodules.
```sh
git submodule init
git submodule update --recursive
```

With a native install of LLVM:
```sh
mkdir build; cd build
cmake ..
```

Without a native install of LLVM:
```sh
# Build LLVM.
cd thirdparty/llvm-project
mkdir build; cd build
cmake ../llvm -DCMAKE_BUILD_TYPE=Debug
make

# Build nxsan-instrumentation-cxx
cd ../../../
mkdir build; cd build
cmake .. -DLLVM_DIR=$(realpath ../thirdparty/llvm-project/build/cmake/Modules)
make
```
