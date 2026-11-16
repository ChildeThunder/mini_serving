编译和运行方式
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

cmake --build build -j

./build/tcp_echo