#pragma once
#include <stdint.h>
#include <string.h>
#define ECO_CAP 24
typedef struct { uint32_t id; int16_t rssi; uint8_t channel, misses; } eco_tree;
typedef struct { eco_tree trees[ECO_CAP]; uint16_t total; uint8_t added, removed; } eco_scene;
static inline uint32_t eco_id(const uint8_t mac[6]) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < 6; ++i) h = (h ^ mac[i]) * 16777619u;
    return h ? h : 1;
}
static inline int eco_height(int rssi) {
    if (rssi < -95) rssi = -95;
    if (rssi > -30) rssi = -30;
    return 8 + (rssi + 95) * 30 / 65;
}
/* Identity, placement and species never depend on scan ordering. Two missed
 * successful scans remove a tree; failures must not call this function. */
static inline void eco_update(eco_scene *s, const eco_tree *input, unsigned count, unsigned total) {
    s->added = s->removed = 0;
    s->total = (uint16_t)total;
    for (unsigned i = 0; i < ECO_CAP; ++i) {
        eco_tree *t = &s->trees[i];
        if (!t->id) continue;
        unsigned j = 0;
        while (j < count && input[j].id != t->id) ++j;
        if (j < count) {
            t->rssi = (int16_t)((3 * t->rssi + input[j].rssi) / 4);
            t->channel = input[j].channel;
            t->misses = 0;
        } else if (++t->misses >= 2) { memset(t, 0, sizeof(*t)); ++s->removed; }
    }
    for (unsigned j = 0; j < count; ++j) {
        if (!input[j].id) continue;
        unsigned i = 0;
        while (i < ECO_CAP && s->trees[i].id != input[j].id) ++i;
        if (i < ECO_CAP) continue;
        for (i = 0; i < ECO_CAP; ++i) if (!s->trees[i].id) {
            s->trees[i] = input[j]; s->trees[i].misses = 0; ++s->added; break;
        }
    }
}
