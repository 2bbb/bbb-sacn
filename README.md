# bbb-sacn

Header-only C++17 helpers for sACN / ANSI E1.31 DMX packet construction and parsing.

This repository is used by [`bbb.artnet`](https://github.com/2bbb/bbb.artnet), but it is intentionally independent so the sACN protocol layer can be tested without Max/MSP or Cycling '74 min-api dependencies.

## What is included

- `bbb/sacn/protocol.hpp`
  - sACN packet constants and layout checks
  - DMX packet construction helpers
  - DMX packet parser
  - universe-to-multicast address helper
  - no external dependency beyond the C++ standard library and platform byte-order/socket headers
- `bbb/sacn/sacn_packet.h`
  - compatibility include for older `bbb.artnet` code
- `bbb/sacn/transport.hpp`
  - minimal blocking UDP sender/receiver helpers
  - multicast membership helpers
  - local bind-interface and multicast-interface configuration
- `bbb/sacn/net_compat.hpp`
  - small cross-platform IPv4 adapter helper used by the transport layer

## Requirements

- CMake 3.19+
- C++17 compiler
- Windows link libraries for tests: `ws2_32`, `iphlpapi`

CI builds and tests on Linux, macOS, and Windows.

## Quick start

```cpp
#include <bbb/sacn/protocol.hpp>

#include <array>
#include <cstdint>
#include <vector>

int main() {
    std::array<uint8_t, 16> cid = bbb::sacn::generate_cid();
    std::array<uint8_t, 3> dmx = {{255, 128, 0}};

    std::vector<uint8_t> packet = bbb::sacn::build_dmx_packet(
        cid.data(),
        "my-source",
        100,
        1,
        13,
        dmx.data(),
        static_cast<int>(dmx.size())
    );

    bbb::sacn::dmx_data parsed{};
    return bbb::sacn::parse_dmx(packet.data(), static_cast<int>(packet.size()), parsed) ? 0 : 1;
}
```

## Use with CMake

As a subdirectory:

```cmake
add_subdirectory(path/to/bbb-sacn)
target_link_libraries(your_target PRIVATE bbb::sacn)
```

`bbb::sacn` links the required Windows socket libraries when used through CMake.

## Build and test

```sh
cmake -B build -DBBB_SACN_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

On Windows, use a Visual Studio generator if needed:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DBBB_SACN_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## CMake options

| Option | Default | Description |
|---|---:|---|
| `BBB_SACN_BUILD_TESTS` | `OFF` | Build CTest targets. |

The exported CMake target is an interface library:

```cmake
bbb::sacn
```

## Provenance

This is an independent, self-contained implementation of sACN / ANSI E1.31 packet encoding and parsing.

No source code from ETCLabs/sACN, OLA, or other third-party sACN implementations is included.

Do not describe this repository as a formal clean-room implementation unless a separate clean-room process is actually performed and documented.

## Scope and limitations

This repository is deliberately small. It is not a complete sACN controller application.

Current limitations:

- The protocol layer is header-only and low-level; it does not own live UDP sockets.
- UDP send/receive policy, threading, and Max/MSP object behavior remain in consumers such as `bbb.artnet`.
- Network behavior is intentionally minimal; CI compile-tests the UDP transport and opens local sockets, but does not require external network fixtures.

## License

MIT License. See [LICENSE](LICENSE) for details.
