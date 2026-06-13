#pragma once

// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 2bit

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace bbb { namespace sacn {

constexpr int acn_packet_id_offset = 4;
constexpr int root_pdu_offset = 16;
constexpr int root_vector_offset = 18;
constexpr int framing_pdu_offset = 38;
constexpr int framing_vector_offset = 40;
constexpr int dmp_pdu_offset = 115;
constexpr int sequence_offset = 111;
constexpr int universe_offset = 113;
constexpr int dmp_vector_offset = 117;
constexpr int property_value_count_offset = 123;
constexpr int property_values_offset = 125;
constexpr int dmx_data_offset = 126;
constexpr int max_dmx_data_length = 512;
constexpr int udp_port = 5568;

inline int clamp_data_length(int length) {
    if(length < 0) {
        return 0;
    }
    return (std::min)(length, max_dmx_data_length);
}

inline int packet_size_for_data_length(int length) {
    return dmx_data_offset + clamp_data_length(length);
}

inline uint16_t read_be16(const uint8_t* buffer, int offset) {
    uint16_t value{0};
    std::memcpy(&value, buffer + offset, sizeof(value));
    return ntohs(value);
}

inline uint32_t read_be32(const uint8_t* buffer, int offset) {
    uint32_t value{0};
    std::memcpy(&value, buffer + offset, sizeof(value));
    return ntohl(value);
}

#pragma pack(push, 1)

struct packet {
    uint8_t preamble_size[2];
    uint8_t postamble_size[2];
    uint8_t acn_packet_id[12];
    uint16_t flags_length1;
    uint32_t root_vector;
    uint8_t cid[16];
    uint16_t flags_length2;
    uint32_t framing_vector;
    uint8_t source_name[64];
    uint8_t priority;
    uint16_t reserved;
    uint8_t sequence;
    uint8_t options;
    uint16_t universe;
    uint16_t flags_length3;
    uint8_t dmp_vector;
    uint8_t address_type;
    uint16_t first_property;
    uint16_t address_increment;
    uint16_t property_value_count;
    uint8_t property_values[513];
};

#pragma pack(pop)

static_assert(offsetof(packet, sequence) == sequence_offset, "sACN sequence offset mismatch");
static_assert(offsetof(packet, acn_packet_id) == acn_packet_id_offset, "sACN ACN packet identifier offset mismatch");
static_assert(offsetof(packet, root_vector) == root_vector_offset, "sACN root vector offset mismatch");
static_assert(offsetof(packet, framing_vector) == framing_vector_offset, "sACN framing vector offset mismatch");
static_assert(offsetof(packet, universe) == universe_offset, "sACN universe offset mismatch");
static_assert(offsetof(packet, dmp_vector) == dmp_vector_offset, "sACN DMP vector offset mismatch");
static_assert(offsetof(packet, property_value_count) == property_value_count_offset, "sACN property count offset mismatch");
static_assert(offsetof(packet, property_values) == property_values_offset, "sACN property values offset mismatch");
static_assert(sizeof(packet) == dmx_data_offset + max_dmx_data_length, "sACN packet size mismatch");

inline std::array<uint8_t, 16> generate_cid() {
    std::array<uint8_t, 16> cid{};
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<int> distribution(0, 255);

    for(auto &byte : cid) {
        byte = static_cast<uint8_t>(distribution(generator));
    }

    cid[6] = (cid[6] & 0x0F) | 0x40;
    cid[8] = (cid[8] & 0x3F) | 0x80;
    return cid;
}

inline void init_packet(
    packet &packet_value,
    const uint8_t cid[16],
    const char* source_name,
    uint8_t priority,
    uint8_t sequence,
    uint16_t universe,
    const uint8_t* data,
    int length
) {
    int data_length = clamp_data_length(length);
    int packet_size = packet_size_for_data_length(data_length);
    int property_value_count = data_length + 1;

    std::memset(&packet_value, 0, sizeof(packet_value));
    packet_value.preamble_size[0] = 0x00;
    packet_value.preamble_size[1] = 0x10;
    packet_value.postamble_size[0] = 0x00;
    packet_value.postamble_size[1] = 0x00;

    const uint8_t acn_id[12] = {
        0x41, 0x53, 0x43, 0x2D, 0x45, 0x31,
        0x2E, 0x31, 0x37, 0x00, 0x00, 0x00
    };
    std::memcpy(packet_value.acn_packet_id, acn_id, sizeof(acn_id));

    uint16_t root_length = static_cast<uint16_t>(packet_size - root_pdu_offset);
    packet_value.flags_length1 = htons(static_cast<uint16_t>(0x7000 | root_length));
    packet_value.root_vector = htonl(0x00000004);
    std::memcpy(packet_value.cid, cid, 16);

    uint16_t framing_length = static_cast<uint16_t>(packet_size - framing_pdu_offset);
    packet_value.flags_length2 = htons(static_cast<uint16_t>(0x7000 | framing_length));
    packet_value.framing_vector = htonl(0x00000002);
    std::strncpy(reinterpret_cast<char*>(packet_value.source_name), source_name, 63);
    packet_value.source_name[63] = 0;
    packet_value.priority = priority;
    packet_value.sequence = sequence;
    packet_value.universe = htons(universe);

    uint16_t dmp_length = static_cast<uint16_t>(packet_size - dmp_pdu_offset);
    packet_value.flags_length3 = htons(static_cast<uint16_t>(0x7000 | dmp_length));
    packet_value.dmp_vector = 0x02;
    packet_value.address_type = 0xA1;
    packet_value.first_property = htons(0x0000);
    packet_value.address_increment = htons(0x0001);
    packet_value.property_value_count = htons(static_cast<uint16_t>(property_value_count));
    packet_value.property_values[0] = 0x00;

    if(data != nullptr && 0 < data_length) {
        std::memcpy(packet_value.property_values + 1, data, static_cast<size_t>(data_length));
    }
}

inline std::vector<uint8_t> build_dmx_packet(
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

    int packet_size = packet_size_for_data_length(length);
    std::vector<uint8_t> result(static_cast<size_t>(packet_size));
    std::memcpy(result.data(), &packet_value, result.size());
    return result;
}

struct multicast_addr {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
};

inline multicast_addr universe_to_multicast(uint16_t universe) {
    return {
        239,
        255,
        static_cast<uint8_t>((universe >> 8) & 0xFF),
        static_cast<uint8_t>(universe & 0xFF)
    };
}

struct dmx_data {
    uint16_t universe{0};
    uint8_t sequence{0};
    uint8_t priority{0};
    const uint8_t* data{nullptr};
    int length{0};
};

inline bool parse_dmx(const uint8_t* buffer, int length, dmx_data &result) {
    if(buffer == nullptr || length < dmx_data_offset) {
        return false;
    }

    const uint8_t acn_id[12] = {
        0x41, 0x53, 0x43, 0x2D, 0x45, 0x31,
        0x2E, 0x31, 0x37, 0x00, 0x00, 0x00
    };
    if(std::memcmp(buffer + acn_packet_id_offset, acn_id, sizeof(acn_id)) != 0) {
        return false;
    }

    if(read_be32(buffer, root_vector_offset) != 0x00000004) {
        return false;
    }

    if(read_be32(buffer, framing_vector_offset) != 0x00000002) {
        return false;
    }

    if(buffer[dmp_vector_offset] != 0x02) {
        return false;
    }

    uint16_t property_value_count = read_be16(buffer, property_value_count_offset);
    if(property_value_count <= 1) {
        return false;
    }

    int available_data_length = length - dmx_data_offset;
    if(available_data_length <= 0) {
        return false;
    }

    int declared_data_length = static_cast<int>(property_value_count) - 1;
    int data_length = (std::min)({declared_data_length, available_data_length, max_dmx_data_length});

    result.universe = read_be16(buffer, universe_offset);
    result.sequence = buffer[sequence_offset];
    result.priority = buffer[108];
    result.data = buffer + dmx_data_offset;
    result.length = data_length;
    return true;
}

}} // namespace bbb::sacn

namespace sacn = bbb::sacn;
