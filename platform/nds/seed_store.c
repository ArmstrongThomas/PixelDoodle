#include "seed_store.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#define fsync _commit
#define fileno _fileno
#else
#include <unistd.h>
#endif
static bool path(char out[256],const char *dir,const char *name)
{
    int n=snprintf(out,256,"%s/%s",dir,name);
    return n>0 && n<256;
}
static bool absent(const char *p)
{
    struct stat info;
    /* fopen/rename below must succeed too; inaccessible files cannot be used. */
    return stat(p,&info)!=0;
}
static bool read_seed(const char *p,unsigned char seed[SEED_BYTES])
{
    FILE *f=fopen(p,"rb"); if(!f) return false;
    bool ok=fread(seed,1,SEED_BYTES,f)==SEED_BYTES;
    if(fgetc(f)!=EOF || ferror(f)) ok=false;
    if(fclose(f)) ok=false;
    return ok;
}
bool seed_claim(const char *dir,unsigned char seed[SEED_BYTES])
{
    char current[256],used[256],temp[256];
    if(!path(current,dir,"seed.bin") || !path(used,dir,"consuming.bin") ||
       !path(temp,dir,"next.bin")) return false;
    if(!absent(used) || !absent(temp)) return false;
    /* Invalidate the old seed before it can be used for TLS. A failed read or
     * crash leaves the claim in place; never automatically replay it. */
    if(rename(current,used)) return false;
    return read_seed(used,seed);
}
bool seed_commit(const char *dir,const unsigned char next[SEED_BYTES])
{
    char current[256],used[256],temp[256]; unsigned char check[SEED_BYTES]={0};
    if(!path(current,dir,"seed.bin") || !path(used,dir,"consuming.bin") ||
       !path(temp,dir,"next.bin") || absent(used) || !absent(current) || !absent(temp)) return false;
    FILE *f=fopen(temp,"wb"); if(!f) return false;
    bool ok=fwrite(next,1,SEED_BYTES,f)==SEED_BYTES;
    if(fflush(f) || fsync(fileno(f))) ok=false;
    if(fclose(f)) ok=false;
    if(!ok || rename(temp,current)) return false;
    ok=read_seed(current,check) && !memcmp(check,next,SEED_BYTES);
    volatile unsigned char *wipe=check;
    for(size_t i=0;i<sizeof(check);i++) wipe[i]=0;
    /* Do not clear the claim until the successor has been verified. */
    if(!ok || remove(used)) return false;
    return true;
}
