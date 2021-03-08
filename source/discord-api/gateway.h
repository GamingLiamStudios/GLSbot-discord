#pragma once

#include "types.h"

#include <string_view>
#include <atomic>

#include "util/types.h"
#include "websocket/ws.h"

namespace discordAPI
{
    class Gateway
    {
    private:
        WebSocket        ws;
        std::string_view bot_token;
        std::atomic_bool resume;

        enum class Opcodes : u16
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
            HeartbeatACK,

            // Gateway Close Event Codes
            UnknownError = 4000,
            UnknownOpcode,
            DecodeError,
            NotAuthenticated,
            AuthenticatedFailed,
            AlreadyAuthenticated,
            InvalidSeq = 4007,
            RateLimited,
            SessionTimeout,
            InvalidShard,
            ShardingRequired,
            InvalidAPIVersion,
            InvalidIntent,
            DisallowedIntent
        };

        std::string session_id;

        struct
        {
            Snowflake   id;
            std::string username;
            char        discrim[4];
        } user;

        std::atomic_uint64_t prev_seqnum;
        std::atomic_bool     ACK;

        void heartbeat(u64 interval);

        int connect(std::string_view bot_token);

    public:
        Gateway(std::string_view bot_token);
    };
}    // namespace discordAPI