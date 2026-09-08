#include <assert.h>
#include <stdio.h>
#include "main/ecology_model.h"
#include "main/ecology_name.h"
int main(void) {
    eco_scene s = {0};
    eco_tree a[] = {{1,-80,1,0,"Home"},{2,-40,11,0,"Cafe"}};
    eco_update(&s,a,2,2); assert(s.added==2 && s.total==2);
    eco_tree b[] = {{2,-60,6,0,"Cafe-new"},{1,-40,1,0,"Home"}};
    eco_update(&s,b,2,2);
    assert(s.added==0 && s.trees[0].id==1 && s.trees[0].rssi==-70);
    assert(s.trees[1].channel==6);
    assert(!strcmp(eco_channel_summary(&s,6).peak->ssid,"Cafe-new"));
    eco_update(&s,b,1,1); assert(s.removed==0 && s.trees[0].misses==1);
    eco_update(&s,b,2,2); assert(s.trees[0].misses==0);
    eco_update(&s,NULL,0,0); eco_update(&s,NULL,0,0);
    assert(s.removed==2 && !s.trees[0].id);
    assert(eco_height(-110)==8 && eco_height(-10)==60);
    for(int r=-95;r<-30;r++) assert(eco_height(r)<=eco_height(r+1));
    eco_tree many[40];
    for(int i=0;i<40;i++) many[i]=(eco_tree){(uint32_t)i+1,-60,6,0,"Shared"};
    eco_update(&s,many,40,40); assert(s.added==24 && s.total==40);
    eco_update(&s,many,40,40); assert(!s.added && !s.removed);
    eco_channel c=eco_channel_summary(&s,6);
    assert(c.count==24 && c.rssi==-60 && !c.stale);
    assert(eco_channel_summary(&s,1).count==0);
    s.trees[3].rssi=-32;
    assert(eco_channel_summary(&s,6).rssi==-32);
    strcpy(s.trees[3].ssid,"Strongest");
    assert(!strcmp(eco_channel_summary(&s,6).peak->ssid,"Strongest"));
    s.trees[4].rssi=-32; assert(eco_channel_summary(&s,6).peak->id==4);
    eco_update(&s,NULL,0,0); assert(eco_channel_summary(&s,6).stale);
    eco_update(&s,NULL,0,0); assert(!eco_channel_summary(&s,6).count);
    assert(eco_channel_x(0)==eco_channel_x(1) && eco_channel_x(15)==eco_channel_x(14));
    for(unsigned ch=1;ch<14;ch++) assert(eco_channel_x(ch+1)-eco_channel_x(ch)==7);
    assert(eco_channel_x(1)-3>=20 && eco_channel_x(14)+3<120);
    uint8_t mac[6]={1,2,3,4,5,6}; assert(eco_id(mac)==eco_id(mac) && eco_id(mac)!=0);
    const unsigned char *utf=(const unsigned char *)"A\xe6\xa3\xae";
    assert(eco_next_char(&utf)=='A' && eco_next_char(&utf)==0x68ee && !*utf);
    utf=(const unsigned char *)"\xe6"; assert(eco_next_char(&utf)=='?' && !*utf);
    utf=(const unsigned char *)"\xed\xa0\x80"; assert(eco_next_char(&utf)=='?');
    assert(eco_glyph_index(0x4e00)==64 && eco_glyph_index(0x9fff)==21055);
    assert(eco_glyph_index(0xffef)==21295 && eco_glyph_index(0x1f332)==-1);
    puts("Ecology model PASS: identity, smoothing, disappearance, limits, RSSI.");
}
