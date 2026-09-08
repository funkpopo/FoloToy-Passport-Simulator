#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
/* Streaming validator for the seven string fields emitted by the shared editor.
 * Does not allocate the embedded image strings on the ESP32 heap. */
typedef struct {
    unsigned state, seen, field, count, unicode, utf8;
    unsigned char utf8_min, utf8_max;
    bool escaped, nonempty;
    char key[12];
} badge_json_t;
static inline bool badge_json_feed(badge_json_t *p, unsigned char c) {
    static const char *keys[] = {"name", "role", "company", "wechat", "bio", "avatar", "qr"};
    static const unsigned limits[] = {96, 144, 144, 192, 228, 220000, 220000};
    if (p->state == 5) {
        if (p->utf8) {
            if (c < p->utf8_min || c > p->utf8_max) return false;
            p->utf8--; p->utf8_min = 0x80; p->utf8_max = 0xbf;
            return ++p->count <= limits[p->field];
        }
        if (p->unicode) { if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false; p->unicode--; }
        else if (p->escaped) { p->escaped = false; if (c == 'u') p->unicode = 4; else if (!strchr("\"\\/bfnrt", c) || !c) return false; }
        else if (c == '\\') p->escaped = true;
        else if (c == '"') { if (p->field == 0 && !p->nonempty) return false; p->seen |= 1u << p->field; p->state = 6; return true; }
        else if (c < 32) return false;
        else if (c >= 0x80) {
            p->utf8_min = 0x80; p->utf8_max = 0xbf;
            if (c >= 0xc2 && c <= 0xdf) p->utf8 = 1;
            else if (c >= 0xe0 && c <= 0xef) { p->utf8 = 2; if (c == 0xe0) p->utf8_min = 0xa0; if (c == 0xed) p->utf8_max = 0x9f; }
            else if (c >= 0xf0 && c <= 0xf4) { p->utf8 = 3; if (c == 0xf0) p->utf8_min = 0x90; if (c == 0xf4) p->utf8_max = 0x8f; }
            else return false;
        }
        if (c != ' ') p->nonempty = true;
        return ++p->count <= limits[p->field];
    }
    if (p->state == 2) {
        if (c == '"') {
            p->key[p->count] = 0;
            for (p->field = 0; p->field < 7; p->field++) if (!strcmp(p->key, keys[p->field])) break;
            if (p->field == 7 || (p->seen & (1u << p->field))) return false;
            p->state = 3; return true;
        }
        if (c < 'a' || c > 'z' || p->count >= sizeof(p->key) - 1) return false;
        p->key[p->count++] = (char)c; return true;
    }
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') return true;
    switch (p->state) {
        case 0: if (c != '{') return false; p->state = 1; break;
        case 1: if (c != '"') return false; p->state = 2; p->count = 0; break;
        case 3: if (c != ':') return false; p->state = 4; break;
        case 4: if (c != '"') return false; p->state = 5; p->count = 0; p->nonempty = false; break;
        case 6: if (c == ',') p->state = 1; else if (c == '}' && p->seen == 127) p->state = 7; else return false; break;
        default: return false;
    }
    return true;
}
static inline bool badge_json_done(const badge_json_t *p) { return p->state == 7; }
