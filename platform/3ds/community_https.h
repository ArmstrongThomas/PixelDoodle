#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
bool pd_https_post(const char*,const char*,char*,size_t,char*,size_t);
bool pd_https_get(const char*,const char*,unsigned char*,size_t,size_t*,int*,char*,size_t);
bool pd_https_get_public(const char*,unsigned char*,size_t,size_t*,int*,char*,size_t);
bool pd_https_post_bearer(const char*,const char*,const char*,char*,size_t,int*,char*,size_t);
bool pd_https_post_multipart(const char*,const unsigned char*,size_t,const char*,int*,char*,size_t);
void pd_https_shutdown(void);
#ifdef __cplusplus
}
#endif
