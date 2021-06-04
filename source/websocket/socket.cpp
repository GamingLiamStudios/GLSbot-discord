#include "socket.h"

#include <exception>
#include <iostream>
#include <future>

#include <openssl/err.h>

#ifdef PLATFORM_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOGDICAPMASKS        // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES    // VK_*
#define NOWINMESSAGES        // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES          // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS         // SM_*
#define NOMENUS              // MF_*
#define NOICONS              // IDI_*
#define NOKEYSTATES          // MK_*
#define NOSYSCOMMANDS        // SC_*
#define NORASTEROPS          // Binary and Tertiary raster ops
#define NOSHOWWINDOW         // SW_*
#define OEMRESOURCE          // OEM Resource values
#define NOATOM               // Atom Manager routines
#define NOCLIPBOARD          // Clipboard routines
#define NOCOLOR              // Screen colors
#define NOCTLMGR             // Control and Dialog routines
#define NODRAWTEXT           // DrawText() and DT_*
#define NOGDI                // All GDI defines and routines
#define NOKERNEL             // All KERNEL defines and routines
#define NOUSER               // All USER defines and routines
#define NONLS                // All NLS defines and routines
#define NOMB                 // MB_* and MessageBox()
#define NOMEMMGR             // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE           // typedef METAFILEPICT
#ifndef NOMINMAX             // Very quick fix
#define NOMINMAX             // Macros min(a,b) and max(a,b)
#endif
#define NOMSG               // typedef MSG and associated routines
#define NOOPENFILE          // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL            // SB_* and scrolling routines
#define NOSERVICE           // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND             // Sound driver routines
#define NOTEXTMETRIC        // typedef TEXTMETRIC and associated routines
#define NOWH                // SetWindowsHook and WH_*
#define NOWINOFFSETS        // GWL_*, GCL_*, associated routines
#define NOCOMM              // COMM driver routines
#define NOKANJI             // Kanji support stuff.
#define NOHELP              // Help engine interface.
#define NOPROFILER          // Profiler interface.
#define NODEFERWINDOWPOS    // DeferWindowPos routines
#define NOMCX               // Modem Configuration Extensions

#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#ifdef EWOULDBLOCK
#undef EWOULDBLOCK
#endif

#define EWOULDBLOCK WSAEWOULDBLOCK
#define get_error   WSAGetLastError
#elif PLATFORM_UNIX 1
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cerrno>

#define INVALID_SOCKET -1
#define SOCKET_ERROR   -1
#define get_error()    errno
#endif

#ifdef _WINDOWS_

#endif

std::mutex SecureSocketBase::_mutex = {};

namespace
{
#ifdef PLATFORM_WINDOWS
    bool g_winsock_initialized;

    struct WinSockCleanup final
    {
        WinSockCleanup() { g_winsock_initialized = false; }

        ~WinSockCleanup() noexcept
        {
            g_winsock_initialized = false;
            WSACleanup();
        }
    } g_winsock_cleanup;    // RAII class to call WSACleanup on shutdown
#endif

    raw_socket_t new_socket()
    {
#ifdef PLATFORM_WINDOWS
        if (!g_winsock_initialized)    // Init winsock if this is the first created socket
        {
            WSADATA wsData;
            WORD    ver = MAKEWORD(2, 2);

            i32 ws0k = WSAStartup(ver, &wsData);

            if (ws0k != 0) std::__throw_runtime_error("Cannot init winsock");
            g_winsock_initialized = true;
        }
#endif

        return socket(AF_INET, SOCK_STREAM, 0);
    }

    std::string get_error_string(i32 errcode = get_error())
    {
#ifdef PLATFORM_WINDOWS
        std::string msg(256, '\0');

        FormatMessage(
          FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,    // flags
          NULL,                                                          // lpsource
          errcode,                                                       // message id
          MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                     // languageid
          msg.data(),                                                    // output buffer
          msg.size(),                                                    // size of msgbuf, bytes
          NULL);                                                         // va_list of arguments

        if (!msg[0])
            sprintf(msg.data(), "%d", errcode);    // provide error # if no string available

        return msg;
#endif
        return std::to_string(errcode);
    }
}    // namespace

SecureSocketBase::SecureSocketBase() noexcept : _handle(INVALID_SOCKET)
{
}

SecureSocketBase::~SecureSocketBase()
{
    if (is_valid()) { close(); }
}

SecureSocketBase::SecureSocketBase(SecureSocketBase &&other) noexcept : _handle(other._handle)
{
    std::lock_guard<std::mutex> lock(_mutex);
    other._handle = INVALID_SOCKET;
    other.use_tls = false;

    other.tls.ctx = NULL;
    other.tls.ssl = NULL;
}

SecureSocketBase &SecureSocketBase::operator=(SecureSocketBase &&other) noexcept
{
    if (&other != this)
    {
        if (is_valid()) close();
        std::lock_guard<std::mutex> lock(_mutex);

        _handle = other._handle;
        use_tls = other.use_tls;
        tls     = other.tls;

        other._handle = INVALID_SOCKET;
        other.use_tls = false;

        other.tls.ctx = NULL;
        other.tls.ssl = NULL;
    }
    return *this;
}

bool SecureSocketBase::operator==(const SecureSocketBase &other) const noexcept
{
    return _handle == other._handle;
}

bool SecureSocketBase::is_valid() const noexcept
{
    return _handle != INVALID_SOCKET;
}

void SecureSocketBase::close() noexcept
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (use_tls)
    {
        SSL_shutdown(tls.ssl);
        SSL_free(tls.ssl);

        SSL_CTX_free(tls.ctx);

        tls.ssl = NULL;
        tls.ctx = NULL;
        use_tls = false;
    }

#ifdef PLATFORM_WINDOWS
    // See
    // https://docs.microsoft.com/en-us/troubleshoot/windows/win32/close-non-blocked-socket-memory-leak
    shutdown(_handle, SD_BOTH);
    closesocket(_handle);
#elif PLATFORM_UNIX 1
    ::close(_handle);
#endif

    _handle = INVALID_SOCKET;
}

SecureSocketBase::SecureSocketBase(raw_socket_t handle) noexcept : _handle(handle)
{
}

//
// CLIENT SOCKET
//

ClientSocket ClientSocket::connect(std::string address, std::string port, bool secure, bool udp)
{
    addrinfo     hints, *ai, *ai0;
    int          i;
    raw_socket_t fd;

#ifdef PLATFORM_WINDOWS
    if (!g_winsock_initialized)
    {
        WSADATA wsData;
        WORD    ver = MAKEWORD(2, 2);

        i32 ws0k = WSAStartup(ver, &wsData);

        if (ws0k != 0) std::__throw_runtime_error("Cannot init winsock");
        g_winsock_initialized = true;
    }
#endif

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = 0;
    hints.ai_socktype = udp ? SOCK_DGRAM : SOCK_STREAM;
    if ((i = getaddrinfo(address.c_str(), port.c_str(), &hints, &ai0)) != 0)
    {
        printf("Unable to look up IP address: %s\n", gai_strerror(i));
        return ClientSocket::invalid();
    }

    for (ai = ai0; ai != NULL; ai = ai->ai_next)
    {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == INVALID_SOCKET)
        {
            std::cout << "Unable to create socket! Error: \n" << get_error_string() << "\n";
            continue;
        }

        i32 flag = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(flag)) == -1)
        {
            std::cout << "Could not disable nagle's algorithm! Error: \n"
                      << get_error_string() << "\n";
            continue;
        }

        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == -1)
        {
            std::cout << "Unable to connect! Error: \n" << get_error_string();
#ifdef PLATFORM_WINDOWS
            shutdown(fd, SD_BOTH);
            closesocket(fd);
#elif PLATFORM_UNIX 1
            ::close(fd);
#endif
            continue;
        }
        break;
    }
    if (ai == NULL)
    {
        printf("ERROR: Unable to Connect to specified IP and Port\n");
        return ClientSocket::invalid();
    }

    return ClientSocket::_from_raw_handle(fd, secure);
}

ClientSocket::ClientSocket() noexcept
{
}

i32 ClientSocket::read_bytes(u8 *buf, i32 buf_len) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    i32                         result;
    if (buf_len <= 0 || buf == nullptr) return 0;

    if (use_tls)
        result = SSL_read(tls.ssl, reinterpret_cast<char *>(buf), buf_len);
    else
        result = ::recv(_handle, reinterpret_cast<char *>(buf), buf_len, 0);

    if (result <= 0)
    {
        i32 err;
        if (use_tls)
        {
            err = SSL_get_error(tls.ssl, result);
            ERR_print_errors_fp(stderr);
            fprintf(stderr, "Error occurred while reading bytes from server: %d\n", err);
            return SOCK_ERROR;
        }
        err = get_error();
        std::cerr << "Error occurred while reading bytes from server: \n"
                  << err << ": " << get_error_string(err);
        return SOCK_ERROR;
    }

    return result;
}

i32 ClientSocket::send_bytes(u8 const *buf, i32 buf_len) const noexcept
{
    std::lock_guard<std::mutex> lock(_mutex);
    i32                         result;
    if (buf_len <= 0 || buf == nullptr) return 0;

    if (use_tls)
        result = SSL_write(tls.ssl, reinterpret_cast<char const *>(buf), buf_len);
    else
        result = ::send(_handle, reinterpret_cast<char const *>(buf), buf_len, 0);

    if (result < 0)
    {
        i32 err;
        if (use_tls)
        {
            SSL_get_error(tls.ssl, err);
            ERR_print_errors_fp(stderr);
            fprintf(stderr, "Error occurred while sending bytes to server: %d\n", err);
            return SOCK_ERROR;
        }
        err = get_error();
        std::cerr << "Error occurred while sending bytes to server: \n"
                  << err << ": " << get_error_string(err);
        return SOCK_ERROR;
    }
    return result;
}

bool ClientSocket::is_localhost() const noexcept
{
    sockaddr_in rem_addr {}, loc_addr {};
    u32         len = sizeof(rem_addr);

#ifdef PLATFORM_WINDOWS
    getpeername(_handle, (sockaddr *) &rem_addr, (i32 *) &len);
    getsockname(_handle, (sockaddr *) &loc_addr, (i32 *) &len);
    return (rem_addr.sin_addr.S_un.S_addr == loc_addr.sin_addr.S_un.S_addr);
#elif PLATFORM_UNIX 1
    getpeername(_handle, (sockaddr *) &rem_addr, &len);
    getsockname(_handle, (sockaddr *) &loc_addr, &len);
    return (rem_addr.sin_addr.s_addr == loc_addr.sin_addr.s_addr);
#endif
}

u32 ClientSocket::remaining() const noexcept
{
    u32 bytes_available;
#ifdef PLATFORM_WINDOWS
    ioctlsocket(_handle, FIONREAD, (u_long *) &bytes_available);
#elif PLATFORM_UNIX 1
    ioctl(_handle, FIONREAD, &bytes_available);
#endif
    return bytes_available;
}

ClientSocket ClientSocket::_from_raw_handle(raw_socket_t handle, bool secure)
{
    return ClientSocket(handle, secure);
}

ClientSocket::ClientSocket(raw_socket_t handle, bool secure) noexcept : SecureSocketBase(handle)
{
    if (secure)
    {
        const SSL_METHOD *method = TLS_client_method();
        tls.ctx                  = SSL_CTX_new(method);
        if (tls.ctx == NULL)
        {
            ERR_print_errors_fp(stderr);
            std::__throw_runtime_error("SSL_CTX_new Failed");
        }

        tls.ssl = SSL_new(tls.ctx);
        if (tls.ssl == NULL) std::__throw_runtime_error("SSL_new failed");

        SSL_set_fd(tls.ssl, _handle);
        int status = SSL_connect(tls.ssl);
        if (status != 1)
        {
            SSL_get_error(tls.ssl, status);
            ERR_print_errors_fp(stderr);    // High probability this doesn't do anything
            fprintf(stderr, "SSL_connect failed with SSL_get_error code %d\n", status);
            std::__throw_runtime_error("");
        }

        use_tls = true;
    }
}

ClientSocket ClientSocket::invalid() noexcept
{
    return ClientSocket(INVALID_SOCKET, false);
}
