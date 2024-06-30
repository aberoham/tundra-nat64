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

#pragma once
#include"tundra.h"


extern uint16_t checksum__calculate_ipv4_header_checksum(const struct iphdr *ipv4_header);
extern uint16_t checksum__calculate_checksum_ipv4(const uint8_t *payload1_ptr, const size_t payload1_size, const uint8_t *nullable_payload2_ptr, const size_t zeroable_payload2_size, const struct iphdr *nullable_ipv4_header);
extern uint16_t checksum__calculate_checksum_ipv6(const uint8_t *payload1_ptr, const size_t payload1_size, const uint8_t *nullable_payload2_ptr, const size_t zeroable_payload2_size, const struct ipv6hdr *nullable_ipv6_header, const uint8_t carried_protocol);
extern uint16_t checksum__recalculate_checksum_4to6(const uint16_t old_checksum, const struct iphdr *old_ipv4_header, const struct ipv6hdr *new_ipv6_header);
extern uint16_t checksum__recalculate_checksum_6to4(const uint16_t old_checksum, const struct ipv6hdr *old_ipv6_header, const struct iphdr *new_ipv4_header);
