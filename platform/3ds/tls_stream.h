#ifndef TLS_STREAM_H
#define TLS_STREAM_H

#include <stddef.h>
#include <stdint.h>

// A TLS 1.2 client stream backed by a nonblocking socket. Global TLS state must
// be initialized after socInit() and shut down before socExit(). Each instance
// is intended to be owned by one worker/thread at a time.
class TlsStream
{
public:
    enum ConnectPhase
    {
        CONNECT_PHASE_DNS,
        CONNECT_PHASE_TCP,
        CONNECT_PHASE_TLS_HANDSHAKE
    };

    struct ConnectMetrics
    {
        uint32_t dnsMs;
        uint32_t tcpMs;
        uint32_t tlsHandshakeMs;
        bool resumed;

        ConnectMetrics()
            : dnsMs(0), tcpMs(0), tlsHandshakeMs(0), resumed(false)
        {
        }
    };

    // Called at phase boundaries and while a socket operation is waiting.
    // Returning false cancels the attempt. The callback runs on the owning
    // network/update worker and must not block.
    typedef bool (*ConnectProgressCallback)(ConnectPhase phase,
                                            const ConnectMetrics& metrics,
                                            void* userData);

    enum IoResult
    {
        IO_OK,
        IO_WOULD_BLOCK,
        IO_CLOSED,
        IO_ERROR
    };

    TlsStream();
    ~TlsStream();

    // Idempotent global setup/teardown for the 3DS system RNG and embedded CA
    // trust store. initialize() returns false on any setup or CA parsing error.
    static bool initialize();
    static void shutdown();

    // TCP connect and the verified TLS handshake each receive timeoutMs and
    // should run on a network/update worker. DNS uses the platform resolver.
    bool connect(const char* host, const char* port, int timeoutMs);

    // Phase-specific variant. Keeping a full TLS budget is important on Old
    // 3DS, while a shorter TCP budget avoids one dead route consuming it.
    bool connect(const char* host, const char* port, int tcpTimeoutMs,
                 int tlsHandshakeTimeoutMs, ConnectProgressCallback callback,
                 void* userData);

    const ConnectMetrics& connectMetrics() const;

    // Once connected, I/O is nonblocking and may complete only part of the
    // supplied buffer. A write retried after IO_WOULD_BLOCK must use the same
    // buffer and length until Mbed TLS reports progress.
    IoResult read(void* buffer, size_t capacity, size_t& bytesRead);
    IoResult write(const void* buffer, size_t length, size_t& bytesWritten);

    void close();
    bool isOpen() const;
    const char* lastError() const;

    // Cryptographically secure bytes from a per-stream DRBG seeded exclusively
    // by the checked 3DS system RNG service.
    bool randomBytes(void* buffer, size_t length);

private:
    TlsStream(const TlsStream&);
    TlsStream& operator=(const TlsStream&);

    struct Impl;
    Impl* impl_;
    char lastError_[192];
};

#endif
