#pragma once
#include <stdint.h>
#define BADGE_FRAME_BYTES (240u * 320u * 2u)
#define BADGE_PIXEL_BYTES (BADGE_FRAME_BYTES * 3u)
#define BADGE_MAX_PROFILE 220000u
static inline unsigned badge_next_page(unsigned page, unsigned key) {
    return key == 0 ? (page + 2) % 3 : key == 2 ? (page == 1 ? 2 : 1) : (page + 1) % 3;
}
static inline int badge_valid_size(unsigned size) {
    return size > BADGE_PIXEL_BYTES && size <= BADGE_PIXEL_BYTES + BADGE_MAX_PROFILE;
}
