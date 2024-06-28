/*
Copyright (c) 2024 Vít Labuda. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:
 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
    disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
    following disclaimer in the documentation and/or other materials provided with the distribution.
 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
    products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include"t64_tundra.h"
#include"t64_utils_ip.h"

#include"t64_utils.h"


// Unusable IPv4 address blocks:
// - 0.0.0.0/8 (Current network)
// - 127.0.0.0/8 (Loopback)
// - 224.0.0.0/4 (Multicast)
// - 255.255.255.255/32 (Limited broadcast)
bool t64f_utils_ip__is_ipv4_address_unusable(const uint8_t *ipv4_address) {
    return (bool) (
        (*ipv4_address == 0) || // 0.0.0.0/8 (Current network)
        (*ipv4_address == 127) || // 127.0.0.0/8 (Loopback)
        (*ipv4_address >= 224 && *ipv4_address <= 239) || // 224.0.0.0/4 (Multicast)
        (T64M_UTILS_IP__IPV4_ADDRESSES_EQUAL(ipv4_address, "\xff\xff\xff\xff")) // 255.255.255.255/32 (Limited broadcast)
    );
}

// Unusable IPv6 address blocks:
// - ::/128 (Unspecified address)
// - ::1/128 (Loopback)
// - ff00::/8 (Multicast)
bool t64f_utils_ip__is_ipv6_address_unusable(const uint8_t *ipv6_address) {
    return (bool) (
        (*ipv6_address == 255) || // ff00::/8 (Multicast)
        (T64M_UTILS_IP__IPV6_ADDRESSES_EQUAL(ipv6_address, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00")) || // ::/128 (Unspecified address)
        (T64M_UTILS_IP__IPV6_ADDRESSES_EQUAL(ipv6_address, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01")) // ::1/128 (Loopback)
    );
}

bool t64f_utils_ip__is_ipv4_address_unusable_or_private(const uint8_t *ipv4_address) {
    return (bool) (
        (*ipv4_address == 0) || // 0.0.0.0/8
        (*ipv4_address == 10) || // 10.0.0.0/8
        (*ipv4_address == 100 && (ipv4_address[1] >= 64 && ipv4_address[1] <= 127)) || // 100.64.0.0/10
        (*ipv4_address == 127) || // 127.0.0.0/8
        (*ipv4_address == 169 && ipv4_address[1] == 254) || // 169.254.0.0/16
        (*ipv4_address == 172 && (ipv4_address[1] >= 16 && ipv4_address[1] <= 31)) || // 172.16.0.0/12
        (T64M_UTILS__MEMORY_EQUAL(ipv4_address, "\xc0\x00\x00", 3)) || // 192.0.0.0/24
        (T64M_UTILS__MEMORY_EQUAL(ipv4_address, "\xc0\x00\x02", 3)) || // 192.0.2.0/24
        (T64M_UTILS__MEMORY_EQUAL(ipv4_address, "\xc0\x58\x63", 3)) || // 192.88.99.0/24
        (*ipv4_address == 192 && ipv4_address[1] == 168) || // 192.168.0.0/16
        (*ipv4_address == 198 && (ipv4_address[1] == 18 || ipv4_address[1] == 19)) || // 198.18.0.0/15
        (T64M_UTILS__MEMORY_EQUAL(ipv4_address, "\xc6\x33\x64", 3)) || // 198.51.100.0/24
        (T64M_UTILS__MEMORY_EQUAL(ipv4_address, "\xcb\x00\x71", 3)) || // 203.0.113.0/24
        (*ipv4_address >= 224) // 224.0.0.0/4 & 240.0.0.0/4 (including 255.255.255.255/32)
    );
}

bool t64f_utils_ip__is_ip_protocol_number_forbidden(const uint8_t ip_protocol_number) {
    // https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
    return (bool) (
        (ip_protocol_number == 0) || // IPv6 Hop-by-Hop Option
        (ip_protocol_number == 2) || // IGMP (Internet Group Management Protocol)
        (ip_protocol_number == 43) || // Routing Header for IPv6
        (ip_protocol_number == 44) || // Fragment Header for IPv6
        (ip_protocol_number == 51) || // Authentication Header
        (ip_protocol_number == 60) || // Destination Options for IPv6
        (ip_protocol_number == 135) || // Mobility Header
        (ip_protocol_number == 139) || // Host Identity Protocol
        (ip_protocol_number == 140) // Shim6 Protocol
    );

    // ESP (50) is allowed
    //  (ip_protocol_number == 50) || // Encapsulating Security Payload
}

void t64f_utils_ip__generate_ipv6_fragment_identifier(t64ts_tundra__xlat_thread_context *context, uint8_t *destination) {
    const uint32_t fragment_id = htonl(context->fragment_identifier_ipv6); // This prevents the program from leaking the information about its endianness
    context->fragment_identifier_ipv6++; // htonl() may be a macro

    memcpy(destination, &fragment_id, 4);
}

void t64f_utils_ip__generate_ipv4_fragment_identifier(t64ts_tundra__xlat_thread_context *context, uint8_t *destination) {
    const uint16_t fragment_id = htons(context->fragment_identifier_ipv4); // This prevents the program from leaking the information about its endianness
    context->fragment_identifier_ipv4++; // htons() may be a macro

    memcpy(destination, &fragment_id, 2);
}
