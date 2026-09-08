#include <assert.h>
#include <stdio.h>
#include "main/badge_protocol.h"
#include "main/badge_json.h"
static bool valid(const char *json) {
    badge_json_t parser = {0};
    for (size_t i = 0; json[i]; i++) if (!badge_json_feed(&parser, (unsigned char)json[i])) return false;
    return badge_json_done(&parser);
}
int main(void) {
    assert(badge_next_page(0, 0) == 2); assert(badge_next_page(2, 1) == 0);
    assert(badge_next_page(0, 2) == 1); assert(badge_next_page(1, 2) == 2); assert(badge_next_page(2, 2) == 1);
    assert(!badge_valid_size(BADGE_PIXEL_BYTES)); assert(badge_valid_size(BADGE_PIXEL_BYTES + 2));
    assert(!badge_valid_size(BADGE_PIXEL_BYTES + BADGE_MAX_PROFILE + 1));
    assert(valid("{\"name\":\"Test\",\"role\":\"\",\"company\":\"\",\"wechat\":\"\",\"bio\":\"quote: \\\" \\u4f60\",\"avatar\":\"\",\"qr\":\"\"}"));
    assert(!valid("{\"name\":\"\"}")); assert(!valid("{\"name\":\"a\",\"name\":\"b\"}"));
    assert(!valid("{\"name\":\"bad\\q\"}")); assert(!valid("{\"name\":\"bad\nline\"}"));
    assert(!valid("{\"name\":true}")); assert(!valid("{}"));
    assert(!valid("{\"name\":\"\xc0\xaf\"}"));
    puts("badge protocol tests PASS"); return 0;
}
