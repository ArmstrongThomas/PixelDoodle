#include "https_client.h"
#include "tls_stream.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
static std::string wire, sent, output;
static size_t offset;
static bool trust = true;
TlsStream::TlsStream():impl_(NULL) {}
TlsStream::~TlsStream() {}
bool TlsStream::initialize() { return trust; }
bool TlsStream::connect(const char*,const char*,int) { return true; }
void TlsStream::close() {}
const char* TlsStream::lastError()const { return "mock error"; }
TlsStream::IoResult TlsStream::read(void* b,size_t cap,size_t& n) {
 n=std::min(std::min(cap,size_t(3)),wire.size()-offset);
 if(!n)return IO_CLOSED;
 memcpy(b,wire.data()+offset,n);offset+=n;return IO_OK;
}
TlsStream::IoResult TlsStream::write(const void* b,size_t len,size_t& n) {
 n=std::min(len,size_t(5));sent.append((const char*)b,n);return IO_OK;
}
static bool sink(const unsigned char* b,size_t n,void*) { output.append((const char*)b,n);return true; }
static void check(bool b,const char* message) { if(!b){fprintf(stderr,"FAIL %s\n",message);exit(1);} }
static bool run(const char* response,size_t limit=32,const char* path="/api/device/pair/begin") {
 wire=response;sent.clear();output.clear();offset=0;HttpsResponse r;char error[200];
 return HttpsClient::post("pixels.7db.pw","443",path,"{}",limit,sink,NULL,r,error,sizeof(error));
}
static bool runGetBearer(const char* response,const char* token) {
 wire=response;sent.clear();output.clear();offset=0;HttpsResponse r;char error[200];
 return HttpsClient::getBearer("pixels.7db.pw","443","/api/device/favorites?limit=12",token,32,sink,NULL,r,error,sizeof(error));
}
static bool runPostBearer(const char* response,const char* token,const char* body) {
 wire=response;sent.clear();output.clear();offset=0;HttpsResponse r;char error[200];
 return HttpsClient::postBearer("pixels.7db.pw","443","/api/device/uploads/begin",token,body,50000,32,sink,NULL,r,error,sizeof(error));
}
int main() {
 check(run("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}"),"length response");
 check(output=="{}","payload");
 check(sent.find("POST /api/device/pair/begin HTTP/1.1\r\n")==0 && sent.find("Content-Length: 2\r\n")!=std::string::npos && sent.substr(sent.size()-2)=="{}","partial POST writes");
 check(run("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\n{\r\n1\r\n}\r\n0\r\n\r\n") && output=="{}","fragmented chunks");
 check(run("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}"),"informational");
 check(run("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n{}"),"close-delimited");
 check(!run("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\n{}"),"truncation");
 check(!run("HTTP/1.1 200 OK\r\nContent-Length: 33\r\n\r\n"),"length bound");
 check(!run("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n",2),"chunk bound");
 check(!run("HTTP/1.1 302 Found\r\nLocation: https://pixels.7db.pw/other\r\nContent-Length: 0\r\n\r\n"),"POST redirects rejected");
 check(!run("HTTP/1.1 429 Busy\r\nContent-Length: 0\r\n\r\n"),"error status");
 check(!run("",32,"/bad\r\nX: injected"),"path injection");
 trust=false;check(!run("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}") && sent.empty(),"trust initialization fail closed");
 trust=true;std::string token(43,'A');
 check(runGetBearer("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}",token.c_str()) && output=="{}","authenticated GET");
 check(sent.find("GET /api/device/favorites?limit=12 HTTP/1.1\r\n")==0 && sent.find(("Authorization: Bearer "+token+"\r\n").c_str())!=std::string::npos,"bearer GET header");
 check(!runGetBearer("HTTP/1.1 302 Found\r\nLocation: /elsewhere\r\nContent-Length: 0\r\n\r\n",token.c_str()),"authenticated redirect rejected");
 check(!runGetBearer("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}", "short") && sent.empty(),"invalid bearer rejected");
 check(runPostBearer("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}",token.c_str(),"{\"favorite\":true}"),"authenticated POST");
 check(sent.find(("Authorization: Bearer "+token+"\r\n").c_str())!=std::string::npos,"bearer POST header");
 std::string largerBody(2048,'x');check(runPostBearer("HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n{}",token.c_str(),largerBody.c_str()),"bounded larger authenticated POST");
 puts("HTTPS framing, fragmentation, bearer scopes, upload-sized POST, caps, redirect rejection and fail-closed initialization: PASS");
}
