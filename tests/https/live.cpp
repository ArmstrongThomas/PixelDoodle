// Opt-in Windows integration probe: production HTTP parser and TLS configuration,
// with Windows sockets/RNG replacing the 3DS platform services. Never logs codes.
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include "https_client.h"
#include "tls_stream.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/platform_time.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <iterator>
extern "C" mbedtls_ms_time_t mbedtls_ms_time(void) { return (mbedtls_ms_time_t)GetTickCount64(); }
static bool wrongHostname;
static int rng(void*,unsigned char* out,size_t n) { return BCryptGenRandom(NULL,out,(ULONG)n,BCRYPT_USE_SYSTEM_PREFERRED_RNG)==0?0:-1; }
struct TlsStream::Impl { mbedtls_net_context net; mbedtls_ssl_context ssl; mbedtls_ssl_config config; mbedtls_x509_crt ca; };
TlsStream::TlsStream():impl_(new Impl) {
 mbedtls_net_init(&impl_->net);mbedtls_ssl_init(&impl_->ssl);mbedtls_ssl_config_init(&impl_->config);mbedtls_x509_crt_init(&impl_->ca);lastError_[0]=0;
}
TlsStream::~TlsStream(){close();mbedtls_ssl_free(&impl_->ssl);mbedtls_ssl_config_free(&impl_->config);mbedtls_x509_crt_free(&impl_->ca);delete impl_;}
bool TlsStream::initialize(){return true;}
void TlsStream::close(){mbedtls_net_free(&impl_->net);}
const char* TlsStream::lastError()const{return lastError_;}
bool TlsStream::connect(const char* host,const char* port,int) {
 std::ifstream f("data/cacert.pem",std::ios::binary);std::string pem((std::istreambuf_iterator<char>(f)),{});
 int rc=mbedtls_x509_crt_parse(&impl_->ca,(const unsigned char*)pem.c_str(),pem.size()+1);
 if(rc)goto fail;
 rc=mbedtls_ssl_config_defaults(&impl_->config,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);if(rc)goto fail;
 mbedtls_ssl_conf_authmode(&impl_->config,MBEDTLS_SSL_VERIFY_REQUIRED);
 mbedtls_ssl_conf_ca_chain(&impl_->config,&impl_->ca,NULL);mbedtls_ssl_conf_rng(&impl_->config,rng,NULL);
 mbedtls_ssl_conf_read_timeout(&impl_->config,20000);
 rc=mbedtls_ssl_setup(&impl_->ssl,&impl_->config);if(rc)goto fail;
 rc=mbedtls_ssl_set_hostname(&impl_->ssl,wrongHostname ? "invalid.example" : host);if(rc)goto fail;
 rc=mbedtls_net_connect(&impl_->net,host,port,MBEDTLS_NET_PROTO_TCP);if(rc)goto fail;
 mbedtls_ssl_set_bio(&impl_->ssl,&impl_->net,mbedtls_net_send,mbedtls_net_recv,mbedtls_net_recv_timeout);
 rc=mbedtls_ssl_handshake(&impl_->ssl);if(rc)goto fail;
 return true;
 fail:snprintf(lastError_,sizeof(lastError_),"TLS probe error -%04X",-rc);return false;
}
TlsStream::IoResult TlsStream::read(void* b,size_t cap,size_t& n){int rc=mbedtls_ssl_read(&impl_->ssl,(unsigned char*)b,cap);n=rc>0?rc:0;return rc>0?IO_OK:rc==0||rc==MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY?IO_CLOSED:IO_ERROR;}
TlsStream::IoResult TlsStream::write(const void* b,size_t len,size_t& n){int rc=mbedtls_ssl_write(&impl_->ssl,(const unsigned char*)b,len);n=rc>0?rc:0;return rc>0?IO_OK:IO_ERROR;}
static bool sink(const unsigned char* b,size_t n,void* p){((std::string*)p)->append((const char*)b,n);return true;}
int main(){std::string body;HttpsResponse r;char error[200];
 bool ok=HttpsClient::post("pixels.7db.pw","443","/api/device/pair/begin","{\"label\":\"TLS integration probe\",\"requestedScopes\":[\"gallery:read\"]}",1024,sink,&body,r,error,sizeof(error));
 if(!ok){fprintf(stderr,"%s\n",error);return 1;}
 if(body.find("\"deviceCode\":")==std::string::npos||body.find("\"userCode\":")==std::string::npos){puts("Invalid begin response");return 1;}
 size_t start=body.find("\"deviceCode\":\"");start+=14;size_t end=body.find('"',start);
 std::string poll="{\"deviceCode\":\""+body.substr(start,end-start)+"\"}";body.clear();
 ok=HttpsClient::post("pixels.7db.pw","443","/api/device/pair/poll",poll.c_str(),1024,sink,&body,r,error,sizeof(error));
 if(!ok || body.find("authorization_pending")==std::string::npos){puts("Pending poll failed");return 1;}
 wrongHostname=true;body.clear();
 if(HttpsClient::post("pixels.7db.pw","443","/api/device/pair/begin","{}",1024,sink,&body,r,error,sizeof(error))){puts("Hostname verification failed open");return 1;}
 puts("Live pending poll and wrong-hostname rejection: PASS");
 puts("Live verified Mbed TLS + production POST parser + pairing begin: PASS (no approval or credentials persisted)");
}
