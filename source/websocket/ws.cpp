#include "ws.h"

#include <cmath>
#include <cassert>
#include <string>
#include <string_view>
#include <cstdlib>
#include <unordered_map>
#include <iostream>
#include <thread>
#include <cstring>
#include <queue>

#include "util/order.h"

#include <openssl/sha.h>
#include <libbase64.h>

namespace
{
    // From
    // https://www.programmingnotes.org/6504/c-how-to-make-map-unordered-map-keys-case-insensitive-using-c/
    struct case_insensitive_unordered_map
    {
        struct comp
        {
            bool operator()(const std::string &lhs, const std::string &rhs) const
            {
                if (lhs.size() != rhs.size()) return false;
                return ::strncasecmp(lhs.data(), rhs.data(), lhs.size()) == 0;
            }
        };
        struct hash
        {
            unsigned operator()(std::string str) const
            {
                for (unsigned index = 0; index < str.size(); ++index)
                {
                    auto ch    = static_cast<unsigned char>(str[index]);
                    str[index] = static_cast<unsigned char>(std::tolower(ch));
                }
                return std::hash<std::string> {}(str);
            }
        };
    };

    inline std::string Base64Encode(const std::string_view &data)
    {
        const size_t in_len  = data.size();
        size_t       out_len = 4 * ((in_len + 2) / 3);
        std::string  ret(out_len, '\0');

        ::base64_encode(data.data(), in_len, ret.data(), &out_len, 0);
        return ret;
    }

    inline void dump_buffer(unsigned n, const unsigned char *buf)
    {
        int on_this_line = 0;
        while (n-- > 0)
        {
            fprintf(stderr, "%02X ", *buf++);
            on_this_line += 1;
            if (on_this_line == 16 || n == 0)
            {
                int i;
                fputs(" ", stderr);
                for (i = on_this_line; i < 16; i++) fputs(" ", stderr);
                for (i = on_this_line; i > 0; i--) fputc(isprint(buf[-i]) ? buf[-i] : '.', stderr);
                fputs("\n", stderr);
                on_this_line = 0;
            }
        }
    }

}    // namespace

void WebSocket::WSFrame::mask()
{
    if (this->header.mask)
    {
        assert(
          ("ERROR: MSB in ext_payload_length is set.",
           this->dynamic.ext_payload_length & (1 << 63)));
        size_t len = (this->header.payload_length <= 125) ? this->header.payload_length
                                                          : this->dynamic.ext_payload_length;
        for (size_t i = 0; i < len; i++)
            this->dynamic.payload_data[i] ^= ((u8 *) &this->dynamic.masking_key)[i & 0b11];
    }
}

void WebSocket::send_frame(Opcode opcode, u8 *data, size_t data_len)
{
    if (!connected)
    {
        std::cout << "Attempted Frame Send when not connected\n";
        return;
    }

    std::queue<WSFrame> enqueue_buffer;

    if (data_len == 0)
    {
        WSFrame frame;
        frame.header.fin              = true;
        frame.header.mask             = false;
        frame.header.opcode           = opcode;
        frame.header.rsv              = 0;
        frame.dynamic.app_data_offset = 0;    // Since Application Data isn't being used

        frame.header.payload_length = 0;
        frame.dynamic.payload_data  = nullptr;

        frame.dynamic.ext_payload_length = 0;
        frame.dynamic.masking_key        = std::rand() << 16 | std::rand();
        frame.mask();

        enqueue_buffer.push(frame);
    }
    else
    {
        size_t index = 0;

        WSFrame frame;
        for (size_t i = 0; i < data_len; i += 4096)
        {
            frame                         = {};
            frame.header.fin              = false;
            frame.header.mask             = true;
            frame.header.opcode           = Opcode::continuation_frame;
            frame.header.rsv              = 0;
            frame.dynamic.app_data_offset = 0;    // Since Application Data isn't being used

            size_t len = std::min(data_len - i, size_t(4096));
            if (len > 0x7D)
            {
                frame.header.payload_length      = (len > 0xFFFF) ? 127 : 126;
                frame.dynamic.ext_payload_length = len;
            }
            else
                frame.header.payload_length = len;

            // Should I move to using the same pointer with different offsets instead of using
            // lots of mini-mallocs?
            // TODO
            frame.dynamic.payload_data = new u8[len];
            memcpy(frame.dynamic.payload_data, data + i, len);
            // std::cout << frame.dynamic.payload_data;

            frame.dynamic.masking_key = std::rand() << 16 | std::rand();
            frame.mask();

            enqueue_buffer.push(frame);
        }

        enqueue_buffer.front().header.opcode = opcode;
        enqueue_buffer.back().header.fin     = true;
    }

    // Send frames
    WSFrame out;
    while (!enqueue_buffer.empty())
    {
        out = enqueue_buffer.front();
        enqueue_buffer.pop();

        const size_t len = (out.header.payload_length <= 125) ? out.header.payload_length
                                                              : out.dynamic.ext_payload_length;

        u64 reversed_epl = out.dynamic.ext_payload_length;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        std::reverse((u8 *) &reversed_epl, (u8 *) &reversed_epl + 8);
#elif defined(__BYTE_ORDER_RUNTIME__)
        if (is_system_little_endian()) std::reverse((u8 *) &reversed_epl, (u8 *) &reversed_epl + 8);
#endif

        // TODO: Don't allocate every frame, use global buffer
        u8 *   send_buffer = new u8[14 + len];
        size_t index       = 2;

        memcpy(send_buffer, (u8 *) &out.header, 2);
        if (out.dynamic.ext_payload_length > 0)
        {
            memcpy(
              send_buffer + index,
              (u8 *) &reversed_epl + ((out.header.payload_length == 127) ? 0 : 6),
              (out.header.payload_length == 127) ? 8 : 2);
            index += (out.header.payload_length == 127) ? 8 : 2;
        }
        if (out.header.mask)
        {
            memcpy(send_buffer + index, (u8 *) &out.dynamic.masking_key, 4);
            index += 4;
        }
        memcpy(send_buffer + index, out.dynamic.payload_data, len);
        index += len;

        socket.send_bytes(send_buffer, index);
        // std::cout << "send bytes\n";
        // dump_buffer(index, send_buffer);

        delete[] send_buffer;
        delete[] out.dynamic.payload_data;
    }

    // if (data_len != 0) std::cout << "Frame Sent. Opcode: " << std::to_string((u8) opcode) <<
    // "\n";
}

void WebSocket::listen()
{
    i32                  error = 1;
    std::vector<WSFrame> fragment_queue;
    while (connected)
    {
        while (socket.remaining() > 0)
        {
            u8      tmp[8];
            WSFrame cur;
            if (error = socket.read_bytes((u8 *) &cur.header, 2) < 0) break;

            cur.dynamic.ext_payload_length = 0;
            cur.dynamic.masking_key        = 0;
            switch (cur.header.payload_length)
            {
            case 126:
            {
                error                          = socket.read_bytes(tmp, 2);
                cur.dynamic.ext_payload_length = (u16) tmp[0] << 8 | (u16) tmp[1];
            }
            break;
            case 127:
            {
                error                          = socket.read_bytes(tmp, 8);
                cur.dynamic.ext_payload_length = (u64) tmp[0] << 56 | (u64) tmp[1] << 48 |
                  (u64) tmp[0] << 40 | (u64) tmp[1] << 32 | (u64) tmp[4] << 24 |
                  (u64) tmp[5] << 16 | (u64) tmp[6] << 8 | (u64) tmp[7];
            }
            break;
            }
            if (error < 0) break;
            if (cur.header.mask)
                if (error = socket.read_bytes((u8 *) &cur.dynamic.masking_key, 4) < 0) break;

            assert(
              ("ERROR: MSB in ext_payload_length is set.",
               cur.dynamic.ext_payload_length & (1 << 63)));

            u64 len = (cur.header.payload_length <= 125) ? cur.header.payload_length
                                                         : cur.dynamic.ext_payload_length;

            cur.dynamic.payload_data = new u8[len];
            u64 temp_val             = 0;
            do {
                if ((len - temp_val) <= 0) break;
                error = socket.read_bytes(cur.dynamic.payload_data + temp_val, len - temp_val);
                if (error < 0) break;
                temp_val += error;
            } while (temp_val < len);
            if (error < 0) break;
            if (temp_val != len) std::__throw_runtime_error("Read bytes != packet len");

            cur.mask();

            if (!cur.header.fin || cur.header.opcode == Opcode::continuation_frame)
                fragment_queue.push_back(std::move(cur));
            else
            {
                IFrame frame;
                frame.app_data_offset = 0;
                frame.opcode          = cur.header.opcode;
                frame.rsv             = cur.header.rsv;

                frame.payload_length = len;
                frame.payload_data   = std::make_unique<u8[]>(len);
                std::memcpy(frame.payload_data.get(), cur.dynamic.payload_data, len);

                delete[] cur.dynamic.payload_data;

                inbound_queue.enqueue(std::move(frame));
            }

            if (fragment_queue.size() > 1 && fragment_queue.back().header.fin)
            {
                IFrame frame;

                frame.app_data_offset = 0;
                frame.opcode          = fragment_queue.front().header.opcode;
                frame.rsv             = fragment_queue.front().header.rsv;

                for (auto &frag : fragment_queue)
                    frame.payload_length += (frag.header.payload_length <= 125)
                      ? frag.header.payload_length
                      : frag.dynamic.ext_payload_length;
                frame.payload_data = std::make_unique<u8[]>(frame.payload_length);

                size_t index = 0;
                for (auto &frag : fragment_queue)
                {
                    len = (frag.header.payload_length <= 125) ? frag.header.payload_length
                                                              : frag.dynamic.ext_payload_length;
                    memcpy(frame.payload_data.get() + index, frag.dynamic.payload_data, len);
                    index += len;

                    delete[] frag.dynamic.payload_data;
                }

                inbound_queue.enqueue(std::move(frame));
                fragment_queue.clear();
            }
        }
    }
    if (error < 0)
    {
        std::string err_str = "ERROR! " + std::to_string(error);
        std::__throw_runtime_error(err_str.c_str());
    }
}

void WebSocket::close()
{
    send_frame(Opcode::connection_close, NULL, 0);
    connected.store(false);
}

WebSocket::WebSocket(std::string uri, bool udp)
{
    if (uri.empty()) std::__throw_invalid_argument("URI null");

    bool secure = uri.substr(0, 6) == "wss://";
    if (!secure && uri.substr(0, 5) != "ws://") std::__throw_invalid_argument("URI invalid");

    std::string port = secure ? "443" : "80";
    size_t      i1 = uri.find(':', secure ? 6 : 5), i2 = uri.find('/', secure ? 6 : 5);
    if (i1 != std::string::npos && i1 < i2) port = uri.substr(i1, i2 - i1);

    std::string host     = uri.substr(secure ? 6 : 5, std::min(i1, i2) - (secure ? 6 : 5));
    std::string resource = uri.substr(i2);

    socket = ClientSocket::connect(host, port, secure, udp);
    if (!socket.is_valid())
    {
        std::cerr << "Failed to connect to Server\n";
        return;
    }

    {
        std::string http_get;
        http_get.append("GET " + resource + " HTTP/1.1\r\n");
        if (i1 != std::string::npos)
            http_get.append("Host: " + host + ":" + port + "\r\n");
        else
            http_get.append("Host: " + host + "\r\n");

        http_get.append("Upgrade: WebSocket\r\nConnection: Upgrade\r\n");

        auto rand = std::string(16, '0');
        for (auto &c : rand) c = ::rand();
        std::string base64 = Base64Encode(rand);
        http_get.append("Sec-WebSocket-Key: " + base64 + "\r\n");
        http_get.append("Sec-WebSocket-Version: 13\r\n\r\n");

        socket.send_bytes((u8 *) http_get.data(), http_get.size());

        std::string read(4096, '0');
        size_t      len = socket.read_bytes((u8 *) read.data(), 4096);
        if (len < 0)
        {
            std::cerr << "Socket failed at reading\n";
            close();
            return;
        }
        read.resize(len);

        if (read.substr(0, 8) != "HTTP/1.1")
        {
            std::cerr << "Malformed HTTP Response\n";
            close();
            return;
        }
        auto status = ::atoi(read.substr(8).data());
        if (status > 400)
        {
            std::cerr << "HTTP Response Failure\n";
            close();
            return;
        }
        if (status != 101)
        {
            std::cout << "Status != 101. Response:\n";
            std::cout << read;
            close();
            return;
        }

        std::unordered_map<
          std::string,
          std::string_view,
          case_insensitive_unordered_map::hash,
          case_insensitive_unordered_map::comp>
               headers;
        size_t index = read.find('\n') + 1;
        while (true)
        {
            size_t nindex = read.find('\n', index);
            if (nindex == std::string::npos || nindex > len) break;
            auto   str = std::string_view(read.data() + index, nindex - index - 1);
            size_t t   = str.find(':');
            if (t == std::string_view::npos || t > str.length() || (t + 2) > str.length()) break;
            headers.emplace(str.substr(0, t), str.substr(t + 2));
            index = nindex + 1;
            if (index > len) break;
        }

        auto iequals = [](std::string_view &lhs, std::string_view &rhs) -> bool
        {
            if (lhs.size() != rhs.size()) return false;
            return ::strncasecmp(lhs.data(), rhs.data(), lhs.size()) == 0;
        };

        std::string_view cmp = "WebSocket";
        if (headers.find("Upgrade") != headers.end() && !iequals(headers.at("Upgrade"), cmp))
        {
            std::cerr << "Upgrade invalid\n";
            std::cerr << "Upgrade: " << headers.at("Upgrade") << "\n";
            close();
            return;
        }

        cmp = "Upgrade";
        if (headers.find("Connection") != headers.end() && !iequals(headers.at("Connection"), cmp))
        {
            std::cerr << "Connection invalid\n";
            std::cerr << headers.at("Connection") << "\n";
            close();
            return;
        }

        if (headers.find("sec-websocket-accept") != headers.end())
        {
            std::string md(20, '0');
            base64 += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            ::SHA1((unsigned char *) base64.data(), base64.size(), (unsigned char *) md.data());

            if (headers["Sec-WebSocket-Accept"] != Base64Encode(md))
            {
                std::cerr << "Sec-WebSocket-Accept invalid\n";
                close();
                return;
            }
        }
        else
        {
            std::cerr << "Sec-WebSocket-Accept missing\n";
            close();
            return;
        }

        if (headers.find("Sec-WebSocket-Extensions") != headers.end())
        {
            std::cerr << "Sec-WebSocket-Extensions exists\n";
            close();
            return;
        }

        if (headers.find("Sec-WebSocket-Protocol") != headers.end())
        {
            std::cerr << "Sec-WebSocket-Protocol exists\n";
            close();
            return;
        }
    }

    connected.store(true);
    std::thread(&WebSocket::listen, this).detach();    // TODO: Single Threaded
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Successfully Connected to WebSocket\n";
}

size_t WebSocket::iqueue_sizeapprox()
{
    return inbound_queue.size_approx();
}

std::vector<WebSocket::IFrame> WebSocket::dump_iqueue()
{
    std::vector<IFrame> dump;

    dump.resize(inbound_queue.size_approx() + 1);
    size_t ret = inbound_queue.try_dequeue_bulk(dump.begin(), dump.size());
    dump.resize(ret);

    return dump;
}

WebSocket::WebSocket(WebSocket &&other) noexcept
{
    if (other.socket.is_valid())
    {
        other.connected.store(false);
        other.inbound_queue.~ConcurrentQueue();
        other.socket = ClientSocket();
    }
}

WebSocket &WebSocket::operator=(WebSocket &&other) noexcept
{
    if (&other != this)
    {
        if (socket.is_valid()) socket.close();

        socket        = std::move(other.socket);
        inbound_queue = std::move(other.inbound_queue);
        connected.store(other.connected.load());

        other.connected.store(false);
        std::thread(&WebSocket::listen, this).detach();
    }
    return *this;
}
