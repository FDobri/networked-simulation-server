# Networked Simulation Server

C++20 fixed-timestep UDP simulation for up to 64 clients. The server advances a bounded world at 60 Hz and sends FlatBuffers snapshots at 20 Hz. The load executable opens separate loopback sockets and drives scripted inputs at 30 Hz.
