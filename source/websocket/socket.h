#pragma once

#include <memory>
#include <optional>
#include "util/types.h"

#include <openssl/ssl.h>

#if defined(_WIN32)
#define PLATFORM_WINDOWS
using raw_socket_t = u64;
#elif defined(unix) || defined(__unix__) || defined(__unix) || \
  (defined(__APPLE__) && defined(__MACH__))
#define PLATFORM_UNIX
using raw_socket_t = i32;
#else
#error "Unknown platform"
#endif

inline constexpr i32 SOCK_ERROR       = -1;
inline constexpr i32 SOCK_CONN_CLOSED = 0;

// RAII abstraction over platform-dependent socket stuff
class SecureSocketBase
{
public:
    SecureSocketBase() noexcept;    // Initialized to 'invalid' state

    ~SecureSocketBase();

    SecureSocketBase(const SecureSocketBase &) = delete;
    SecureSocketBase &operator=(const SecureSocketBase &) = delete;

    SecureSocketBase(SecureSocketBase &&) noexcept;
    SecureSocketBase &operator=(SecureSocketBase &&) noexcept;

    bool operator==(const SecureSocketBase &other) const noexcept;

    bool is_valid() const noexcept;

    void close() noexcept;

protected:
    explicit SecureSocketBase(raw_socket_t handle) noexcept;

    raw_socket_t _handle;
    struct TLS_CTX
    {
        SSL_CTX *ctx;
        SSL *    ssl;
    };
    bool    use_tls;
    TLS_CTX tls;
};

class ClientSocket final : public SecureSocketBase
{
public:
    static ClientSocket
      connect(std::string address, std::string port, bool secure, bool udp = false);

    ClientSocket() noexcept;    // Initializes to 'invalid'

    i32 read_bytes(u8 *buf, i32 buf_len) const;

    i32 send_bytes(u8 const *buf, i32 buf_len) const noexcept;

    bool is_localhost() const noexcept;

    u32 remaining() const noexcept;

private:
    static ClientSocket _from_raw_handle(raw_socket_t _handle, bool secure);

    explicit ClientSocket(raw_socket_t, bool) noexcept;

    static ClientSocket invalid() noexcept;
};
