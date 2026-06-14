#include <bbb/sacn/transport.hpp>

#include <array>
#include <cstdint>
#include <iostream>

namespace {

int failures{0};

void expect_true(bool condition, const char* message) {
    if(!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

void expect_equal(const std::string &actual, const std::string &expected, const char* message) {
    if(actual != expected) {
        std::cerr << "FAIL: " << message << " actual=" << actual << " expected=" << expected << std::endl;
        ++failures;
    }
}

void test_address_helpers() {
    expect_true(bbb::sacn::is_ipv4_multicast("239.255.0.13"), "sACN multicast address");
    expect_true(!bbb::sacn::is_ipv4_multicast("127.0.0.1"), "loopback is not multicast");
    expect_true(!bbb::sacn::is_ipv4_multicast("not-an-ip"), "invalid IP is not multicast");
    expect_equal(bbb::sacn::multicast_ip_for_universe(13), "239.255.0.13", "universe 13 multicast IP");
    expect_equal(bbb::sacn::multicast_ip_for_universe(256), "239.255.1.0", "universe 256 multicast IP");
}

void test_sender_open_and_packet_send_to_loopback() {
    bbb::sacn::sender sender;
    expect_true(sender.open(), "sender open");

    const std::array<uint8_t, 16> cid{};
    const std::array<uint8_t, 1> data{255};
    expect_true(sender.send_dmx("127.0.0.1", cid.data(), "bbb-sacn-test", 100, 1, 1, data.data(), 1), "send DMX to loopback");
    sender.close();
}

void test_receiver_open_on_ephemeral_port() {
    bbb::sacn::receiver receiver;
    bbb::sacn::receiver_config config;
    config.port = 0;
    expect_true(receiver.open(config), "receiver open on ephemeral port");
    receiver.close();
}

}

int main() {
    test_address_helpers();
    test_sender_open_and_packet_send_to_loopback();
    test_receiver_open_on_ephemeral_port();
    return failures == 0 ? 0 : 1;
}
