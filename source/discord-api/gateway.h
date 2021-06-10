#pragma once

#include <string_view>
#include <queue>
#include <atomic>

#include "util/types.h"
#include "websocket/ws.h"

#include <json/json.hpp>

namespace discordAPI
{
    struct gateway_event
    {
        std::string    name;
        nlohmann::json data;
    };

    class Gateway
    {
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
            PresenceUpdate,
            VoiceStateUpdate,
            Resume = 6,
            Reconnect,
            RequestGuildMembers,
            InvalidSession,
            Hello,
            HeartbeatACK
        };

        std::atomic_uint64_t prev_seqnum;
        std::atomic_bool     ACK;

        std::queue<gateway_event> next_events;

        void heartbeat(u64 interval);

    public:
        Gateway() = default;

        int connect(std::string_view bot_token);

        int           get_incoming();
        gateway_event next_event();

        bool connected();
        void close();
    };
}    // namespace discordAPI