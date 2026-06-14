#pragma once

// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 2bit

#include <bbb/sacn/net_compat.hpp>
#include <bbb/sacn/protocol.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace bbb { namespace sacn {

inline bool parse_ipv4_address(const std::string &ip_string, struct in_addr &address) {
    return inet_pton(AF_INET, ip_string.c_str(), &address) == 1;
}

inline bool is_ipv4_multicast(const std::string &ip_string) {
    struct in_addr address{};
    if(!parse_ipv4_address(ip_string, address)) {
        return false;
    }

    uint32_t host_address = ntohl(address.s_addr);
    return 0xE0000000 <= host_address && host_address <= 0xEFFFFFFF;
}

inline std::string ipv4_address_to_string(uint32_t network_order_address) {
    struct in_addr address{};
    address.s_addr = network_order_address;

    char buffer[INET_ADDRSTRLEN]{};
    if(inet_ntop(AF_INET, &address, buffer, sizeof(buffer)) == nullptr) {
        return {};
    }
    return buffer;
}

inline std::string multicast_ip_for_universe(uint16_t universe) {
    auto multicast = universe_to_multicast(universe);
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%d.%d.%d.%d",
        multicast.a, multicast.b, multicast.c, multicast.d);
    return buffer;
}

inline std::string resolve_bind_ip_for_target(const std::string &destination_ip) {
    if(destination_ip.empty() || is_ipv4_multicast(destination_ip)) {
        return {};
    }

    struct in_addr target{};
    if(!parse_ipv4_address(destination_ip, target)) {
        return {};
    }

    auto adapters = net::get_adapters();
    for(const auto &adapter : adapters) {
        if(!adapter.is_up) {
            continue;
        }
        if((adapter.addr & adapter.netmask) == (target.s_addr & adapter.netmask)) {
            return ipv4_address_to_string(adapter.addr);
        }
    }

    return {};
}

#ifdef _WIN32
using socket_type = SOCKET;
constexpr socket_type invalid_socket_value = INVALID_SOCKET;
#else
using socket_type = int;
constexpr socket_type invalid_socket_value = -1;
#endif

struct sender_config {
    std::string bind_ip;
    int multicast_ttl{4};
    bool multicast_loop{true};
};

class sender {
public:
    sender() = default;

    sender(const sender &) = delete;
    sender &operator=(const sender &) = delete;

    sender(sender &&other) noexcept
    : socket_{other.socket_}
    , last_error_{std::move(other.last_error_)}
    {
        other.socket_ = invalid_socket_value;
    }

    sender &operator=(sender &&other) noexcept {
        if(this != &other) {
            close();
            socket_ = other.socket_;
            last_error_ = std::move(other.last_error_);
            other.socket_ = invalid_socket_value;
        }
        return *this;
    }

    ~sender() {
        close();
    }

    bool open(const sender_config &config = {}) {
        close();
        net::ensure_init();

        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if(!valid()) {
            return fail("failed to create socket");
        }

        if(!set_multicast_ttl(config.multicast_ttl)) {
            return false;
        }
        if(!set_multicast_loop(config.multicast_loop)) {
            return false;
        }

        if(!config.bind_ip.empty() && config.bind_ip != "0.0.0.0") {
            if(!bind_to_interface(config.bind_ip)) {
                return false;
            }
            if(!set_multicast_interface(config.bind_ip)) {
                return false;
            }
        }

        last_error_.clear();
        return true;
    }

    void close() {
        if(valid()) {
            net::close_socket(socket_);
            socket_ = invalid_socket_value;
        }
    }

    bool valid() const {
        return net::socket_valid(socket_);
    }

    const std::string &last_error() const {
        return last_error_;
    }

    bool send_packet(const std::string &destination_ip, const void *data, int length) {
        if(!valid()) {
            return fail("socket is not open");
        }
        if(destination_ip.empty()) {
            return fail("destination IP is empty");
        }
        if(data == nullptr || length <= 0) {
            return fail("packet data is empty");
        }

        struct sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(udp_port);
        if(!parse_ipv4_address(destination_ip, address.sin_addr)) {
            return fail("invalid destination IP: " + destination_ip);
        }

        int sent = ::sendto(socket_, reinterpret_cast<const char*>(data), length, 0,
            reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
        if(sent != length) {
            return fail("failed to send packet");
        }

        last_error_.clear();
        return true;
    }

    bool send_dmx(
        const std::string &destination_ip,
        const uint8_t cid[16],
        const char* source_name,
        uint8_t priority,
        uint8_t sequence,
        uint16_t universe,
        const uint8_t* data,
        int length
    ) {
        packet packet_value{};
        init_packet(packet_value, cid, source_name, priority, sequence, universe, data, length);
        return send_packet(destination_ip, &packet_value, packet_size_for_data_length(length));
    }

private:
    bool fail(const std::string &message) {
        last_error_ = message;
        return false;
    }

    bool set_multicast_ttl(int ttl) {
        if(::setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL,
            reinterpret_cast<const char*>(&ttl), sizeof(ttl)) < 0) {
            return fail("failed to set multicast TTL");
        }
        return true;
    }

    bool set_multicast_loop(bool enabled) {
        char loop = enabled ? 1 : 0;
        if(::setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
            return fail("failed to set multicast loop");
        }
        return true;
    }

    bool bind_to_interface(const std::string &bind_ip) {
        struct in_addr interface_address{};
        if(!parse_ipv4_address(bind_ip, interface_address)) {
            return fail("invalid bind IP: " + bind_ip);
        }

        struct sockaddr_in local_address{};
        local_address.sin_family = AF_INET;
        local_address.sin_port = htons(0);
        local_address.sin_addr = interface_address;

        if(::bind(socket_, reinterpret_cast<struct sockaddr*>(&local_address), sizeof(local_address)) < 0) {
            return fail("failed to bind to " + bind_ip);
        }
        return true;
    }

    bool set_multicast_interface(const std::string &bind_ip) {
        struct in_addr interface_address{};
        if(!parse_ipv4_address(bind_ip, interface_address)) {
            return fail("invalid multicast interface IP: " + bind_ip);
        }

        if(::setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF,
            reinterpret_cast<const char*>(&interface_address), sizeof(interface_address)) < 0) {
            return fail("failed to set multicast interface " + bind_ip);
        }
        return true;
    }

    socket_type socket_{invalid_socket_value};
    std::string last_error_;
};

struct receiver_config {
    uint16_t port{static_cast<uint16_t>(udp_port)};
    std::string bind_ip;
};

class receiver {
public:
    receiver() = default;

    receiver(const receiver &) = delete;
    receiver &operator=(const receiver &) = delete;

    receiver(receiver &&other) noexcept
    : socket_{other.socket_}
    , last_error_{std::move(other.last_error_)}
    {
        other.socket_ = invalid_socket_value;
    }

    receiver &operator=(receiver &&other) noexcept {
        if(this != &other) {
            close();
            socket_ = other.socket_;
            last_error_ = std::move(other.last_error_);
            other.socket_ = invalid_socket_value;
        }
        return *this;
    }

    ~receiver() {
        close();
    }

    bool open(const receiver_config &config = {}) {
        close();
        net::ensure_init();

        socket_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if(!valid()) {
            return fail("failed to create socket");
        }

        int reuse = 1;
        ::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#ifdef SO_REUSEPORT
        ::setsockopt(socket_, SOL_SOCKET, SO_REUSEPORT,
            reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif

        struct sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(config.port);
        if(config.bind_ip.empty() || config.bind_ip == "0.0.0.0") {
            address.sin_addr.s_addr = htonl(INADDR_ANY);
        } else if(!parse_ipv4_address(config.bind_ip, address.sin_addr)) {
            return fail("invalid bind IP: " + config.bind_ip);
        }

        if(::bind(socket_, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
            return fail("failed to bind receiver socket");
        }

        last_error_.clear();
        return true;
    }

    void close() {
        if(valid()) {
            net::close_socket(socket_);
            socket_ = invalid_socket_value;
        }
    }

    bool valid() const {
        return net::socket_valid(socket_);
    }

    const std::string &last_error() const {
        return last_error_;
    }

    bool join_multicast_group(uint16_t universe, const std::string &interface_ip = {}) {
        return update_multicast_group(IP_ADD_MEMBERSHIP, universe, interface_ip, "join");
    }

    bool leave_multicast_group(uint16_t universe, const std::string &interface_ip = {}) {
        return update_multicast_group(IP_DROP_MEMBERSHIP, universe, interface_ip, "leave");
    }

    net::recv_len_t receive(uint8_t* buffer, std::size_t capacity) {
        if(!valid() || buffer == nullptr || capacity == 0) {
            return -1;
        }

        return ::recvfrom(socket_, reinterpret_cast<char*>(buffer), static_cast<int>(capacity), 0, nullptr, nullptr);
    }

private:
    bool fail(const std::string &message) {
        last_error_ = message;
        return false;
    }

    bool update_multicast_group(int option, uint16_t universe, const std::string &interface_ip, const char* verb) {
        if(!valid()) {
            return fail("socket is not open");
        }

        struct ip_mreq request{};
        std::string multicast_ip = multicast_ip_for_universe(universe);
        if(!parse_ipv4_address(multicast_ip, request.imr_multiaddr)) {
            return fail("invalid multicast IP: " + multicast_ip);
        }

        if(interface_ip.empty() || interface_ip == "0.0.0.0") {
            request.imr_interface.s_addr = htonl(INADDR_ANY);
        } else if(!parse_ipv4_address(interface_ip, request.imr_interface)) {
            return fail("invalid multicast interface IP: " + interface_ip);
        }

        if(::setsockopt(socket_, IPPROTO_IP, option,
            reinterpret_cast<const char*>(&request), sizeof(request)) < 0) {
            return fail(std::string("failed to ") + verb + " multicast group " + multicast_ip);
        }

        last_error_.clear();
        return true;
    }

    socket_type socket_{invalid_socket_value};
    std::string last_error_;
};

}} // namespace bbb::sacn
