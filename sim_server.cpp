#include "net.hpp"
#include <algorithm>
#include <map>
#include <thread>
#include <vector>

namespace {
struct Player {
    std::uint32_t id{}, seq{};
    float x{}, y{};
    int dx{}, dy{};
    Clock::time_point seen;
};
constexpr auto interval = 16667us;
}

int main(int argc, char** argv) try {
    const int port = argc > 1 ? integer(argv[1], 0, 65535) : 40000;
    const int seconds = argc > 2 ? integer(argv[2], 0, 3600) : 0;
    asio::io_context io;
    udp::socket socket(io, udp::endpoint(udp::v4(), static_cast<unsigned short>(port)));
    socket.non_blocking(true);
    std::cout << "port=" << socket.local_endpoint().port() << std::endl;
    std::map<udp::endpoint, Player> players;
    std::vector<wire::Body> bodies;
    bodies.reserve(max_clients);
    Datagram data{};
    flatbuffers::FlatBufferBuilder buffer(1024);
    auto next = Clock::now();
    const auto until = next + std::chrono::seconds(seconds);
    std::uint64_t tick = 0, rx = 0, tx = 0, overruns = 0;
    double total_us = 0, max_us = 0;
    std::uint32_t id = 0;

    while (!seconds || Clock::now() < until) {
        const auto start = Clock::now();
        for (auto it = players.begin(); it != players.end();) {
            if (start - it->second.seen > 3s) it = players.erase(it);
            else ++it;
        }
        for (int i = 0; i < 512; ++i) {
            asio::error_code error;
            udp::endpoint peer;
            const auto size = socket.receive_from(asio::buffer(data), peer, 0, error);
            if (error == asio::error::would_block || error == asio::error::try_again) break;
            if (error && transient(error)) continue;
            if (error) throw asio::system_error(error);
            rx += size;
            const auto* packet = decode(data, size);
            const auto* input = packet ? packet->payload_as_Input() : nullptr;
            if (!input || input->dx() < -1 || input->dx() > 1 ||
                input->dy() < -1 || input->dy() > 1 || !input->sequence()) continue;
            auto player = players.find(peer);
            if (player == players.end()) {
                if (players.size() == max_clients) continue;
                player = players.try_emplace(peer, Player{++id}).first;
            }
            auto& p = player->second;
            if (input->sequence() <= p.seq) continue;
            p.seq = input->sequence();
            p.dx = input->dx();
            p.dy = input->dy();
            p.seen = start;
        }
        ++tick;
        bodies.clear();
        for (auto& [peer, player] : players) {
            player.x = std::clamp(player.x + player.dx * 0.05f, -50.0f, 50.0f);
            player.y = std::clamp(player.y + player.dy * 0.05f, -50.0f, 50.0f);
            bodies.emplace_back(player.id, player.x, player.y);
        }
        if (tick % 3 == 0) for (const auto& [peer, player] : players) {
            buffer.Clear();
            const auto data = buffer.CreateVectorOfStructs(bodies);
            const auto snapshot = wire::CreateSnapshot(buffer, tick, player.seq, player.id, data);
            wire::FinishPacketBuffer(buffer, wire::CreatePacket(buffer, wire::Payload_Snapshot, snapshot.Union()));
            tx += send(socket, buffer, peer);
        }
        const auto elapsed = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
        total_us += elapsed;
        max_us = std::max(max_us, elapsed);
        if (elapsed > interval.count()) ++overruns;
        if (tick % 60 == 0) {
            std::cout << "tick=" << tick << " clients=" << players.size()
                      << " avg_us=" << total_us / 60 << " max_us=" << max_us
                      << " rx_bytes=" << rx << " tx_bytes=" << tx
                      << " overruns=" << overruns << std::endl;
            total_us = max_us = 0;
            rx = tx = overruns = 0;
        }
        next += interval;
        if (next < Clock::now() - 4 * interval) next = Clock::now();
        std::this_thread::sleep_until(next);
    }
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
