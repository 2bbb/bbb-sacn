#include <bbb/sacn/transport.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
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

void test_transport_types_compile() {
    bbb::sacn::sender sender;
    bbb::sacn::receiver receiver;
    bbb::sacn::sender_config sender_config;
    bbb::sacn::receiver_config receiver_config;
    sender_config.multicast_ttl = 4;
    sender_config.multicast_loop = true;
    receiver_config.port = static_cast<uint16_t>(bbb::sacn::udp_port);
    expect_true(!sender.valid(), "default sender is closed");
    expect_true(!receiver.valid(), "default receiver is closed");
}

void test_optional_socket_open() {
    const char* enabled = std::getenv("BBB_SACN_RUN_SOCKET_TESTS");
    if(enabled == nullptr || std::string(enabled) != "1") {
        return;
    }

    bbb::sacn::sender sender;
    expect_true(sender.open(), sender.last_error().empty() ? "sender open" : sender.last_error().c_str());
    sender.close();

    bbb::sacn::receiver receiver;
    bbb::sacn::receiver_config config;
    config.port = 0;
    expect_true(receiver.open(config), receiver.last_error().empty() ? "receiver open on ephemeral port" : receiver.last_error().c_str());
    receiver.close();
}

}

int main() {
    test_address_helpers();
    test_transport_types_compile();
    test_optional_socket_open();
    return failures == 0 ? 0 : 1;
}
