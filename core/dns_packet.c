#include "dns_packet.h"
#include <string.h>

static uint16_t get16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static void put16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v; }

size_t pd_dns_build_a_query(uint8_t *out,size_t cap,uint16_t id,const char *host) {
    if(!out||!host||cap<17)return 0;memset(out,0,12);put16(out,id);put16(out+2,0x0100);put16(out+4,1);size_t at=12;
    const char *p=host;if(!*p)return 0;
    while(*p){const char *end=p;size_t n=0;while(*end&&*end!='.'){unsigned char c=(unsigned char)*end;if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-')||++n>63)return 0;end++;}if(!n||at+1+n+5>cap)return 0;out[at++]=(uint8_t)n;memcpy(out+at,p,n);at+=n;p=end;if(*p=='.'){p++;if(!*p)return 0;}}
    if(at+5>cap)return 0;out[at++]=0;put16(out+at,1);at+=2;put16(out+at,1);at+=2;return at;
}

static bool skip_name(const uint8_t *p,size_t n,size_t *at) {
    size_t pos=*at,consumed=0;bool jumped=false;unsigned hops=0;
    for(;;){if(pos>=n||++hops>32)return false;uint8_t c=p[pos++];if((c&0xc0)==0xc0){if(pos>=n)return false;size_t target=((size_t)(c&0x3f)<<8)|p[pos++];if(target>=n)return false;if(!jumped)consumed=pos-*at;jumped=true;pos=target;continue;}if(c&0xc0)return false;if(c==0){if(!jumped)consumed=pos-*at;*at+=consumed;return true;}if(c>63||pos+c>n)return false;pos+=c;}
}

bool pd_dns_parse_a_response(const uint8_t *p,size_t n,uint16_t expected,uint32_t *ip) {
    if(!p||!ip||n<12||get16(p)!=expected)return false;uint16_t flags=get16(p+2),qd=get16(p+4),an=get16(p+6);if(!(flags&0x8000)||(flags&0x0200)||(flags&0x000f)||qd!=1||!an||an>64)return false;size_t at=12;if(!skip_name(p,n,&at)||at+4>n||get16(p+at)!=1||get16(p+at+2)!=1)return false;at+=4;
    for(unsigned i=0;i<an;i++){if(!skip_name(p,n,&at)||at+10>n)return false;uint16_t type=get16(p+at),classv=get16(p+at+2),rdlen=get16(p+at+8);at+=10;if(at+rdlen>n)return false;if(type==1&&classv==1&&rdlen==4){uint32_t v;memcpy(&v,p+at,4);if(v){*ip=v;return true;}}at+=rdlen;}
    return false;
}
