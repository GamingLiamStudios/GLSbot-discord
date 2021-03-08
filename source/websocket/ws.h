#pragma once

#include <vector>
#include <atomic>

#include "socket.h"
#include "util/types.h"
#include "moodycamel/concurrentqueue.h"

class WebSocket
{
public:
    enum class Opcode : u8
    {
        continuation_frame = 0x0,
        text_frame         = 0x1,
        binary_frame       = 0x2,
        connection_close   = 0x8,
        ping               = 0x9,
        pong               = 0xA
    };

private:
    struct WSFrame    // Web Socket Frame
    {
        // Split for ease of reading
        struct alignas(1)    // Lets hope alignas works how it should
        {
            // Reversed order due to byte layout
            Opcode opcode : 4;
            u8     rsv : 3;
            bool   fin : 1;

            u8   payload_length : 7;
            bool mask : 1;
        } header;

        struct
        {
            u64 ext_payload_length;    // Can be 32 or 64 bits
            u32 masking_key;           // Can be null

            u8 *payload_data;       // Starts at 'Extension data'
            u64 app_data_offset;    // Also the length of 'Extension data'
        } dynamic;

        // Bi-Directional
        void mask();
    };

    struct IFrame    // Internal Frame
    {
        Opcode opcode : 4;
        u8     rsv : 3;
        bool   _ : 1;    // Align to byte

        u64 payload_length;
        u8 *payload_data;
        u64 app_data_offset;
    };

    moodycamel::ConcurrentQueue<IFrame> inbound_queue;

    ClientSocket socket;

    void listen();

public:
    WebSocket() = default;
    WebSocket(std::string uri, bool udp = false);

    WebSocket(const WebSocket &) = delete;
    WebSocket &operator=(const WebSocket &) = delete;

    WebSocket(WebSocket &&) noexcept;
    WebSocket &operator=(WebSocket &&) noexcept;

    void send_frame(Opcode opcode, u8 *data, size_t data_len);
    void close();

    std::vector<IFrame> dump_iqueue();
    size_t              iqueue_sizeapprox();

    std::atomic_bool connected;
};
