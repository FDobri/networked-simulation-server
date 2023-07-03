#pragma once
#include <asio.hpp>
#include "wire_generated.h"
#include <array>
#include <charconv>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>

using asio::ip::udp;
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
constexpr std::size_t max_clients = 64;
using Datagram = std::array<std::uint8_t, 1200>;

inline int integer(std::string_view text, int min, int max) {
    int value{};
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() || value < min || value > max)
        throw std::invalid_argument("numeric argument out of range");
    return value;
}
inline const wire::Packet* decode(const Datagram& data, std::size_t size) {
    flatbuffers::Verifier verifier(data.data(), size, 8, 16);
    return wire::VerifyPacketBuffer(verifier) ? wire::GetPacket(data.data()) : nullptr;
}
