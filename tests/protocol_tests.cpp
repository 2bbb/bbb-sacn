#include <bbb/sacn/protocol.hpp>
#include <bbb/sacn/sacn_packet.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

int failures{0};

void expect_true(bool condition, const char* message) {
    if(!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

void expect_equal(int actual, int expected, const char* message) {
    if(actual != expected) {
        std::cerr << "FAIL: " << message << " actual=" << actual << " expected=" << expected << std::endl;
        ++failures;
    }
}

void test_layout_constants() {
    expect_equal(bbb::sacn::packet_size_for_data_length(512), 638, "full packet size");
    expect_equal(bbb::sacn::packet_size_for_data_length(3), 129, "partial packet size");
    expect_equal(bbb::sacn::packet_size_for_data_length(-10), bbb::sacn::dmx_data_offset, "negative data length clamps to zero");
    expect_equal(bbb::sacn::packet_size_for_data_length(999), 638, "oversized data length clamps to DMX max");

    expect_equal(bbb::sacn::acn_packet_id_offset, 4, "ACN packet identifier offset");
    expect_equal(bbb::sacn::root_vector_offset, 18, "root vector offset");
    expect_equal(bbb::sacn::framing_vector_offset, 40, "framing vector offset");
    expect_equal(bbb::sacn::sequence_offset, 111, "sequence offset");
    expect_equal(bbb::sacn::universe_offset, 113, "universe offset");
    expect_equal(bbb::sacn::dmp_vector_offset, 117, "DMP vector offset");
    expect_equal(bbb::sacn::property_value_count_offset, 123, "property value count offset");
    expect_equal(bbb::sacn::dmx_data_offset, 126, "DMX data offset");
}

void test_packet_generation_and_parsing() {
    bbb::sacn::packet packet{};
    const std::array<uint8_t, 16> cid{
        0x00, 0x01, 0x02, 0x03,
        0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B,
        0x0C, 0x0D, 0x0E, 0x0F
    };
    const std::array<uint8_t, 4> data{10, 20, 30, 40};

    bbb::sacn::init_packet(packet, cid.data(), "bbb.sacn.test", 100, 77, 13, data.data(), static_cast<int>(data.size()));

    const auto* bytes = reinterpret_cast<const uint8_t*>(&packet);
    const uint8_t acn_id[12] = {
        0x41, 0x53, 0x43, 0x2D, 0x45, 0x31,
        0x2E, 0x31, 0x37, 0x00, 0x00, 0x00
    };

    expect_true(std::memcmp(bytes + bbb::sacn::acn_packet_id_offset, acn_id, sizeof(acn_id)) == 0, "ACN packet identifier");
    expect_equal(static_cast<int>(bbb::sacn::read_be32(bytes, bbb::sacn::root_vector_offset)), 0x00000004, "root vector");
    expect_equal(static_cast<int>(bbb::sacn::read_be32(bytes, bbb::sacn::framing_vector_offset)), 0x00000002, "framing vector");
    expect_equal(static_cast<int>(bytes[bbb::sacn::sequence_offset]), 77, "sequence");
    expect_equal(static_cast<int>(bbb::sacn::read_be16(bytes, bbb::sacn::universe_offset)), 13, "universe");
    expect_equal(static_cast<int>(bytes[bbb::sacn::dmp_vector_offset]), 0x02, "DMP vector");
    expect_equal(static_cast<int>(bbb::sacn::read_be16(bytes, bbb::sacn::property_value_count_offset)), 5, "property value count includes start code");
    expect_equal(static_cast<int>(bytes[bbb::sacn::property_values_offset]), 0, "DMX start code");
    expect_equal(static_cast<int>(bytes[bbb::sacn::dmx_data_offset]), 10, "DMX slot 1");
    expect_equal(static_cast<int>(bytes[bbb::sacn::dmx_data_offset + 3]), 40, "DMX slot 4");

    bbb::sacn::dmx_data parsed{};
    expect_true(bbb::sacn::parse_dmx(bytes, bbb::sacn::packet_size_for_data_length(static_cast<int>(data.size())), parsed), "parse generated packet");
    expect_equal(static_cast<int>(parsed.universe), 13, "parsed universe");
    expect_equal(static_cast<int>(parsed.sequence), 77, "parsed sequence");
    expect_equal(parsed.length, 4, "parsed length");
    expect_equal(static_cast<int>(parsed.data[2]), 30, "parsed DMX slot 3");
}

void test_build_vector() {
    const std::array<uint8_t, 16> cid{};
    const std::array<uint8_t, 3> data{1, 2, 3};
    std::vector<uint8_t> packet = bbb::sacn::build_dmx_packet(
        cid.data(), "bbb.sacn.vector", 100, 5, 42, data.data(), static_cast<int>(data.size())
    );

    expect_equal(static_cast<int>(packet.size()), bbb::sacn::packet_size_for_data_length(static_cast<int>(data.size())), "vector packet size");

    bbb::sacn::dmx_data parsed{};
    expect_true(bbb::sacn::parse_dmx(packet.data(), static_cast<int>(packet.size()), parsed), "parse vector packet");
    expect_equal(static_cast<int>(parsed.universe), 42, "vector universe");
    expect_equal(parsed.length, 3, "vector length");
}

void test_consecutive_universe_numbers() {
    const std::array<uint8_t, 16> cid{};
    const std::array<uint8_t, 1> data{255};

    for(int i = 0; i < 13; ++i) {
        bbb::sacn::packet packet{};
        uint16_t universe = static_cast<uint16_t>(1 + i);
        bbb::sacn::init_packet(packet, cid.data(), "bbb.sacn.test", 100, static_cast<uint8_t>(i), universe, data.data(), 1);

        const auto* bytes = reinterpret_cast<const uint8_t*>(&packet);
        expect_equal(static_cast<int>(bbb::sacn::read_be16(bytes, bbb::sacn::universe_offset)),
            static_cast<int>(universe), "consecutive universe number");
    }
}

void test_legacy_namespace_alias() {
    expect_equal(sacn::udp_port, 5568, "legacy sacn namespace alias");
}

}

int main() {
    test_layout_constants();
    test_packet_generation_and_parsing();
    test_build_vector();
    test_consecutive_universe_numbers();
    test_legacy_namespace_alias();
    return failures == 0 ? 0 : 1;
}
