#include <assert.h>
#include <stdio.h>
#include "main/ecology_model.h"
int main(void) {
    eco_scene s = {0};
    eco_tree a[] = {{1,-80,1,0},{2,-40,11,0}};
    eco_update(&s,a,2,2); assert(s.added==2 && s.total==2);
    eco_tree b[] = {{2,-60,6,0},{1,-40,1,0}};
    eco_update(&s,b,2,2);
    assert(s.added==0 && s.trees[0].id==1 && s.trees[0].rssi==-70);
    assert(s.trees[1].channel==6);
    eco_update(&s,b,1,1); assert(s.removed==0 && s.trees[0].misses==1);
    eco_update(&s,b,2,2); assert(s.trees[0].misses==0);
    eco_update(&s,NULL,0,0); eco_update(&s,NULL,0,0);
    assert(s.removed==2 && !s.trees[0].id);
    assert(eco_height(-110)==8 && eco_height(-10)==38);
    for(int r=-95;r<-30;r++) assert(eco_height(r)<=eco_height(r+1));
    eco_tree many[40];
    for(int i=0;i<40;i++) many[i]=(eco_tree){(uint32_t)i+1,-60,6,0};
    eco_update(&s,many,40,40); assert(s.added==24 && s.total==40);
    eco_update(&s,many,40,40); assert(!s.added && !s.removed);
    uint8_t mac[6]={1,2,3,4,5,6}; assert(eco_id(mac)==eco_id(mac) && eco_id(mac)!=0);
    puts("Ecology model PASS: identity, smoothing, disappearance, limits, RSSI.");
}
