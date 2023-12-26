# Networked Simulation Server

C++20 fixed-timestep UDP simulation for up to 64 clients. The server advances a bounded world at 60 Hz and sends FlatBuffers snapshots at 20 Hz. The load executable opens separate loopback sockets and drives scripted inputs at 30 Hz.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
python3 measure.py build/sim_server build/sim_load --counts 1 8 32 64 --seconds 5
```
