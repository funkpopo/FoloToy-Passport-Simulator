#pragma once
#include <stdint.h>
/* Bounded decoder: malformed bytes and unsupported/control characters render
 * as '?'. Never split a multi-byte sequence at the drawing boundary. */
static inline uint32_t eco_next_char(const unsigned char **cursor) {
    const unsigned char *p=*cursor;
    unsigned first=*p++;
    if(first<128) { *cursor=p; return first>=32?first:'?'; }
    unsigned n=first>=0xc2 && first<=0xdf?1:first>=0xe0 && first<=0xef?2:first>=0xf0 && first<=0xf4?3:0;
    if(!n) { *cursor=p; return '?'; }
    uint32_t cp=first & ((1u<<(6-n))-1);
    for(unsigned i=0;i<n;i++) {
        if(!p[i] || (p[i]&0xc0)!=0x80) { *cursor=p; return '?'; }
        cp=(cp<<6)|(p[i]&63);
    }
    *cursor=p+n;
    if((n==1 && cp<0x80)||(n==2 && cp<0x800)||(n==3 && cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)) return '?';
    return cp;
}
static inline int eco_glyph_index(uint32_t cp) {
    if(cp>=0x3000 && cp<=0x303f) return (int)(cp-0x3000);
    if(cp>=0x4e00 && cp<=0x9fff) return 64+(int)(cp-0x4e00);
    if(cp>=0xff00 && cp<=0xffef) return 64+20992+(int)(cp-0xff00);
    return -1;
}
