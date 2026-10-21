#ifndef HTTPS_CLIENT_H
#define HTTPS_CLIENT_H

#include <stddef.h>

struct HttpsResponse
{
    int statusCode;
    long long contentLength;
    size_t bodyBytes;
    bool chunked;
};

typedef bool (*HttpsBodyCallback)(const unsigned char* data, size_t length, void* userData);

// Bounded HTTPS/1.1 client adapted from Collab Doodle. JSON POST requests
// reject redirects and never fall back to plaintext HTTP.
class HttpsClient
{
public:
    static bool post(const char* host, const char* port, const char* path,
                     const char* body, size_t maxBodyBytes, HttpsBodyCallback callback,
                     void* userData, HttpsResponse& response, char* error, size_t errorSize);
    static bool postBinary(const char* host, const char* port, const char* path,
                           const unsigned char* body, size_t bodySize,
                           const char* contentType, size_t maxResponseBytes,
                           HttpsBodyCallback callback, void* userData,
                           HttpsResponse& response, char* error, size_t errorSize);
    static bool get(const char* host, const char* port, const char* path,
                    size_t maxBodyBytes, HttpsBodyCallback bodyCallback,
                    void* userData, HttpsResponse& response,
                    char* error, size_t errorSize, int maxRedirects = 3);
    static bool getBearer(const char* host, const char* port, const char* path,
                          const char* bearer, size_t maxBodyBytes,
                          HttpsBodyCallback bodyCallback, void* userData,
                          HttpsResponse& response, char* error, size_t errorSize);
    static bool postBearer(const char* host, const char* port, const char* path,
                           const char* bearer, const char* body, size_t maxRequestBytes,
                           size_t maxResponseBytes, HttpsBodyCallback callback,
                           void* userData, HttpsResponse& response,
                           char* error, size_t errorSize);
};

#endif
