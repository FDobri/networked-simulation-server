FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates cmake g++ make
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF && cmake --build build -j2
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends libstdc++6 iproute2 python3 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/sim_server /src/build/sim_load /usr/local/bin/
COPY measure.py /opt/sim/
CMD ["sim_server", "40000"]
