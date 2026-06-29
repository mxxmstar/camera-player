// @file media_packet.cpp
// C API media_packet_init / media_packet_clear implementation
#include "media/media_packet.h"
#include "media/media_buffer.h"
#include "media/media_packet.hpp"
#include <cstring>
#include <new>

void media_packet_init(media_packet_t* pkt) {
    if (!pkt) return;
    std::memset(pkt, 0, sizeof(*pkt));
}

void media_packet_clear(media_packet_t* pkt) {
    if (!pkt) return;
    if (pkt->buffer) {
        media_buffer_destroy(pkt->buffer);
    }
    media_packet_init(pkt);
}