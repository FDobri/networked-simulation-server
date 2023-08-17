#include "net.hpp"
#include <algorithm>
#include <numeric>
#include <thread>
#include <vector>

struct Client {
    explicit Client(asio::io_context& io) : socket(io, udp::endpoint(udp::v4(), 0)) {
        socket.non_blocking(true);
    }
    udp::socket socket;
    std::uint64_t last_tick{};
    std::uint32_t last_ack{};
    std::size_t snapshots{}, skipped{};
};

int main(int argc, char** argv) try {
    if (argc != 5) throw std::invalid_argument("usage: sim_load HOST PORT CLIENTS SECONDS");
    const auto endpoint = udp::endpoint(asio::ip::make_address_v4(argv[1]), integer(argv[2], 1, 65535));
    const auto count = integer(argv[3], 1, static_cast<int>(max_clients));
    const auto seconds = integer(argv[4], 1, 3600);
    asio::io_context io;
    std::vector<Client> clients;
    clients.reserve(count);
    for (int n = 0; n < count; ++n) clients.emplace_back(io);
    Datagram data{};
    flatbuffers::FlatBufferBuilder builder;
    std::vector<double> rtt;
    std::vector<Clock::time_point> sent;
    const auto start = Clock::now();
    auto next = start;
    std::uint32_t seq = 0;
    std::size_t tx = 0, rx = 0;
    while (Clock::now() - start < std::chrono::seconds(seconds)) {
        ++seq;
        sent.push_back(Clock::now());
        builder.Clear();
        const auto input = wire::CreateInput(builder, seq, seq / 30 % 2 ? -1 : 1, 0);
        wire::FinishPacketBuffer(builder, wire::CreatePacket(builder, wire::Payload_Input, input.Union()));
        for (auto& client : clients) tx += send(client.socket, builder, endpoint);
        next += 33333us;
        std::this_thread::sleep_until(next);
        const auto received = Clock::now();
        for (auto& client : clients) for (int i = 0; i < 64; ++i) {
            udp::endpoint peer;
            asio::error_code error;
            const auto n = client.socket.receive_from(asio::buffer(data), peer, 0, error);
            if (error == asio::error::would_block || error == asio::error::try_again) break;
            if (error && transient(error)) continue;
            if (error) throw asio::system_error(error);
            if (peer != endpoint) continue;
            rx += n;
            const auto* packet = decode(data, n);
            const auto* snap = packet ? packet->payload_as_Snapshot() : nullptr;
            if (!snap || snap->tick() <= client.last_tick || !snap->bodies() ||
                snap->bodies()->size() > max_clients || !snap->ack() || snap->ack() > seq) continue;
            const auto* bodies = snap->bodies();
            if (std::none_of(bodies->begin(), bodies->end(), [&](const auto* b) {
                    return b->id() == snap->self();
                })) continue;
            if (client.last_tick) client.skipped += (snap->tick() - client.last_tick) / 3 - 1;
            client.last_tick = snap->tick();
            ++client.snapshots;
            if (snap->ack() > client.last_ack) {
                rtt.push_back(std::chrono::duration<double, std::milli>(received - sent[snap->ack() - 1]).count());
                client.last_ack = snap->ack();
            }
        }
    }
    std::sort(rtt.begin(), rtt.end());
    const auto p95 = rtt.empty() ? 0.0 : rtt[(rtt.size() - 1) * 95 / 100];
    std::size_t received = 0, missing = 0, active = 0;
    for (const auto& client : clients) {
        received += client.snapshots;
        missing += client.skipped;
        active += client.snapshots != 0;
    }
    std::cout << "clients=" << count << " active=" << active << " snapshots=" << received
              << " skipped=" << missing << " ack_p95_ms=" << p95
              << " tx_bytes=" << tx << " rx_bytes=" << rx << '\n';
    return active == clients.size() ? 0 : 1;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
