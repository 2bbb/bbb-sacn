// SPDX-License-Identifier: MIT
// Copyright (c) 2025-2026 2bit

#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    #include <stdio.h>

    namespace bbb { namespace sacn { namespace net {
        inline void ensure_init() {
            static bool initialized = false;
            if(!initialized) {
                WSADATA d;
                WSAStartup(MAKEWORD(2,2), &d);
                initialized = true;
            }
        }
        inline void close_socket(SOCKET fd) { closesocket(fd); }
        inline bool socket_valid(SOCKET fd) { return fd != INVALID_SOCKET; }
        using recv_len_t = int;
    }}}

#else
    #include <arpa/inet.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <sys/socket.h>
    #include <netinet/in.h>

    namespace bbb { namespace sacn { namespace net {
        inline void ensure_init() {}
        inline void close_socket(int fd) { ::close(fd); }
        inline bool socket_valid(int fd) { return fd >= 0; }
        using recv_len_t = ssize_t;
    }}}
#endif

#include <string>
#include <cstring>
#include <vector>

namespace bbb { namespace sacn { namespace net {

struct adapter_info {
    std::string name;
    uint32_t addr;
    uint32_t netmask;
    uint32_t broadcast;
    bool is_up;
    bool is_loopback;
};

#ifdef _WIN32
    inline std::vector<adapter_info> get_adapters() {
        std::vector<adapter_info> result;
        ULONG bufLen = 15000;
        auto* buf = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);
        if(!buf) return result;

        ULONG rv = GetAdaptersAddresses(AF_INET,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr, buf, &bufLen);

        if(rv == ERROR_BUFFER_OVERFLOW) {
            free(buf);
            buf = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);
            if(!buf) return result;
            rv = GetAdaptersAddresses(AF_INET,
                GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                nullptr, buf, &bufLen);
        }
        if(rv != NO_ERROR) { free(buf); return result; }

        for(auto* adapter = buf; adapter; adapter = adapter->Next) {
            if(adapter->OperStatus != IfOperStatusUp) continue;
            bool is_loopback = (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK);

            for(auto* ua = adapter->FirstUnicastAddress; ua; ua = ua->Next) {
                if(!ua->Address.lpSockaddr) continue;
                if(ua->Address.lpSockaddr->sa_family != AF_INET) continue;

                adapter_info info;
                char narrow[256] = {};
                wcstombs(narrow, adapter->FriendlyName ? adapter->FriendlyName : L"", sizeof(narrow)-1);
                info.name = narrow;

                auto* sin = (sockaddr_in*)ua->Address.lpSockaddr;
                info.addr = sin->sin_addr.s_addr;
                info.is_up = true;
                info.is_loopback = is_loopback;

                if(ua->OnLinkPrefixLength == 0) {
                    info.netmask = 0;
                } else if(ua->OnLinkPrefixLength <= 32) {
                    info.netmask = htonl(~((1ULL << (32 - ua->OnLinkPrefixLength)) - 1));
                } else {
                    info.netmask = 0;
                }
                info.broadcast = info.addr | ~info.netmask;

                result.push_back(info);
            }
        }
        free(buf);
        return result;
    }
#else
    inline std::vector<adapter_info> get_adapters() {
        std::vector<adapter_info> result;
        struct ifaddrs* ifa_list = nullptr;
        if(getifaddrs(&ifa_list) != 0) return result;

        for(auto* ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
            if(!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;

            adapter_info info;
            info.name = ifa->ifa_name ? ifa->ifa_name : "";
            info.is_up = (ifa->ifa_flags & IFF_UP) != 0;
            info.is_loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0;

            auto* sin = (sockaddr_in*)ifa->ifa_addr;
            info.addr = sin->sin_addr.s_addr;

            if(ifa->ifa_netmask) {
                auto* mask = (sockaddr_in*)ifa->ifa_netmask;
                info.netmask = mask->sin_addr.s_addr;
            } else {
                info.netmask = 0;
            }

            if(ifa->ifa_broadaddr && ifa->ifa_broadaddr->sa_family == AF_INET) {
                auto* bcast = (sockaddr_in*)ifa->ifa_broadaddr;
                info.broadcast = bcast->sin_addr.s_addr;
            } else {
                info.broadcast = info.addr | ~info.netmask;
            }

            result.push_back(info);
        }
        freeifaddrs(ifa_list);
        return result;
    }
#endif

}}} // namespace bbb::sacn::net
