#ifndef PD_DNS_PACKET_H
#define PD_DNS_PACKET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t pd_dns_build_a_query(uint8_t *out, size_t capacity, uint16_t id,
                            const char *hostname);
bool pd_dns_parse_a_response(const uint8_t *packet, size_t length,
                             uint16_t expected_id, uint32_t *ipv4_network);
#endif
