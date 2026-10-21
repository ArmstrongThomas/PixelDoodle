#include "community_https.h"
#include "https_client.h"
#include "tls_stream.h"
#include <3ds.h>
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static void* socketBuffer;
struct Sink { char* data; size_t capacity, size; };
static bool receive(const unsigned char* data, size_t length, void* user) {
    Sink* sink = static_cast<Sink*>(user);
    if (length >= sink->capacity - sink->size) return false;
    memcpy(sink->data + sink->size, data, length); sink->size += length;
    sink->data[sink->size] = 0; return true;
}
static bool discard_response(const unsigned char*, size_t, void*) { return true; }
bool pd_https_post(const char* path,const char* body,char* out,size_t cap,char* error,size_t errorCap) {
    if (!out || cap < 2) return false;
    out[0] = 0;
    if (!socketBuffer) {
        socketBuffer = memalign(0x1000, 1024*1024);
        if (!socketBuffer) { snprintf(error,errorCap,"Network allocation failed"); return false; }
        Result result = socInit(static_cast<u32*>(socketBuffer), 1024*1024);
        if (R_FAILED(result)) {
            free(socketBuffer); socketBuffer = NULL;
            snprintf(error,errorCap,"Socket init %08lX",(unsigned long)result); return false;
        }
    }
    Sink sink = {out,cap,0}; HttpsResponse response;
    return HttpsClient::post("pixels.7db.pw","443",path,body,cap-1,receive,&sink,response,error,errorCap);
}
bool pd_https_get(const char* path,const char* token,unsigned char* out,size_t cap,size_t* outSize,int* status,char* error,size_t errorCap) {
    if(!out||cap<2||!outSize||!status)return false;out[0]=0;*outSize=0;*status=0;
    if(!socketBuffer){socketBuffer=memalign(0x1000,1024*1024);if(!socketBuffer){snprintf(error,errorCap,"Network allocation failed");return false;}Result r=socInit(static_cast<u32*>(socketBuffer),1024*1024);if(R_FAILED(r)){free(socketBuffer);socketBuffer=NULL;snprintf(error,errorCap,"Socket init %08lX",(unsigned long)r);return false;}}
    Sink sink={(char*)out,cap,0};HttpsResponse response;
    bool ok=HttpsClient::getBearer("pixels.7db.pw","443",path,token,cap-1,receive,&sink,response,error,errorCap);
    *outSize=sink.size;*status=response.statusCode;return ok;
}
bool pd_https_get_public(const char* path,unsigned char* out,size_t cap,size_t* outSize,int* status,char* error,size_t errorCap) {
    if(!out||cap<2||!outSize||!status)return false;out[0]=0;*outSize=0;*status=0;
    if(!socketBuffer){socketBuffer=memalign(0x1000,1024*1024);if(!socketBuffer){snprintf(error,errorCap,"Network allocation failed");return false;}Result r=socInit(static_cast<u32*>(socketBuffer),1024*1024);if(R_FAILED(r)){free(socketBuffer);socketBuffer=NULL;snprintf(error,errorCap,"Socket init %08lX",(unsigned long)r);return false;}}
    Sink sink={(char*)out,cap,0};HttpsResponse response;bool ok=HttpsClient::get("pixels.7db.pw","443",path,cap-1,receive,&sink,response,error,errorCap,0);*outSize=sink.size;*status=response.statusCode;return ok;
}
bool pd_https_post_bearer(const char* path,const char* token,const char* body,char* out,size_t cap,int* status,char* error,size_t errorCap) {
    if(!out||cap<2||!status)return false;out[0]=0;*status=0;
    if(!socketBuffer){socketBuffer=memalign(0x1000,1024*1024);if(!socketBuffer){snprintf(error,errorCap,"Network allocation failed");return false;}Result r=socInit(static_cast<u32*>(socketBuffer),1024*1024);if(R_FAILED(r)){free(socketBuffer);socketBuffer=NULL;snprintf(error,errorCap,"Socket init %08lX",(unsigned long)r);return false;}}
    Sink sink={out,cap,0};HttpsResponse response;
    bool ok=HttpsClient::postBearer("pixels.7db.pw","443",path,token,body,50000,cap-1,receive,&sink,response,error,errorCap);
    *status=response.statusCode;return ok;
}
bool pd_https_post_multipart(const char* path,const unsigned char* body,size_t size,const char* contentType,int* status,char* error,size_t errorCap){
    if(!body||!size||!status)return false;*status=0;
    if(!socketBuffer){socketBuffer=memalign(0x1000,1024*1024);if(!socketBuffer){snprintf(error,errorCap,"Network allocation failed");return false;}Result r=socInit(static_cast<u32*>(socketBuffer),1024*1024);if(R_FAILED(r)){free(socketBuffer);socketBuffer=NULL;snprintf(error,errorCap,"Socket init %08lX",(unsigned long)r);return false;}}
    HttpsResponse response;
    bool ok=HttpsClient::postBinary("discord.com","443",path,body,size,contentType,65536,discard_response,NULL,response,error,errorCap);
    *status=response.statusCode;return ok;
}
void pd_https_shutdown(void) {
    TlsStream::shutdown();
    if (socketBuffer) { socExit(); free(socketBuffer); socketBuffer = NULL; }
}
