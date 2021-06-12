#pragma once

#include <string_view>
#include <queue>
#include <atomic>

#include "util/types.h"
#include "websocket/ws.h"

#include <json/json.hpp>

namespace discord
{
    class Gateway
    {
    public:
        struct event
        {
            std::string    name;
            nlohmann::json data;
        };

        enum send_opcodes : u8
        {
            PresenceUpdate = 3,
            VoiceStateUpdate,
            RequestGuildMembers = 8
        };

        Gateway() = default;

        int connect(std::string_view bot_token);

        int   get_incoming();
        event next_event();
        void  send_event(send_opcodes op, std::string_view json);

        bool connected();
        void close();
        void disconnect(u16 op = 1001);

    private:
        WebSocket ws;
        bool      resume;

        std::string_view bot_token;
        std::string      session_id;

        enum class Opcodes : u8
        {
            // Gateway Opcodes
            Dispatch = 0,
            Heartbeat,
            Identify,
            Resume = 6,
            Reconnect,
            InvalidSession = 9,
            Hello,
            HeartbeatACK
        };

        std::atomic_uint64_t prev_seqnum;
        std::atomic_bool     ACK;

        std::queue<event> next_events;

        void heartbeat(u64 interval);
    };
}    // namespace discord