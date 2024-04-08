# Networked Simulation Server

C++20 fixed-timestep UDP simulation for up to 64 clients. The server advances a bounded world at 60 Hz and sends FlatBuffers snapshots at 20 Hz. The load executable opens separate loopback sockets and drives scripted inputs at 30 Hz.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
python3 measure.py build/sim_server build/sim_load --counts 1 8 32 64 --seconds 5
```

On Windows with a multi-config generator, supply `build/Release/sim_server.exe` and `build/Release/sim_load.exe`. The server prints its chosen port when started with port 0. Exit after the optional duration or stop with Ctrl+C.

## Design and measurements

A single event loop owns simulation and network state. Incoming datagrams are verified, inputs have monotonic sequence numbers, stale clients expire after three seconds and the receive budget is bounded per tick. Snapshots are full-state and fit within the 1,200-byte receive buffer. This is a local simulation prototype whose client identity is the UDP endpoint.

The server reports mean and maximum tick work, overruns and UDP payload bytes over 60 ticks. The load client reports observed gaps in snapshot ticks and p95 input acknowledgment latency, including client polling delay. Runs discard packets outside the first and last observed ticks when measuring gaps. Record CPU, OS, compiler, build configuration, duration and `tc` configuration alongside results.

Use a disposable Linux container namespace to apply delay and loss to loopback on both legs. The first example runs a baseline. The next run applies 40 ms delay with 10 ms variation and 2% independent loss per traversed packet.

```sh
docker build -t cpp-sim .
docker run --rm cpp-sim python3 /opt/sim/measure.py /usr/local/bin/sim_server /usr/local/bin/sim_load
docker run --rm --cap-add NET_ADMIN cpp-sim sh -c 'tc qdisc add dev lo root netem delay 40ms 10ms loss 2% && python3 /opt/sim/measure.py /usr/local/bin/sim_server /usr/local/bin/sim_load'
```

## Local results

Windows 10 22H2, Ryzen 7 3700X, MSVC 19.44 x64 Release, 3 seconds per client count on the same host. The acknowledgment metric includes the load client's polling interval and OS scheduling.

| Clients | Snapshots received | Gaps | p95 acknowledgment (ms) |
| ---: | ---: | ---: | ---: |
| 1 | 60 | 0 | 77.53 |
| 8 | 480 | 0 | 77.97 |
| 32 | 1664 | 0 | 125.26 |
| 64 | 3776 | 0 | 108.57 |

The [Network validation workflow](https://github.com/FDobri/networked-simulation-server/actions/workflows/network.yml) runs five-second cases in a Release container on Ubuntu and attaches the baseline and impaired reports to each run.

## Linux delay and packet loss results

[GitHub Actions run 43633090485](https://github.com/FDobri/networked-simulation-server/actions/runs/43633090485) used Ubuntu 24.04, two AMD EPYC 9V74 vCPUs, GCC 13.3 Release in Docker and iproute2 6.1.0. Each case ran for five seconds on loopback. The impaired case configured 40 ms delay, 10 ms variation and 2% loss per traversed packet.

| Clients | Baseline gaps | Impaired gaps | Baseline p95 (ms) | Impaired p95 (ms) |
| ---: | ---: | ---: | ---: | ---: |
| 1 | 0 | 2 | 33.35 | 133.34 |
| 8 | 0 | 25 | 33.35 | 133.34 |
| 32 | 0 | 71 | 33.32 | 133.31 |
| 64 | 0 | 114 | 33.31 | 133.28 |

All clients received snapshots. The workflow artifact contains tick work durations, payload byte counts and the applied queue configuration.
