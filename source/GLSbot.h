#pragma once

#include <string>

#include "websocket/ws.h"

class GLSbot
{
private:
    WebSocket ws;

    struct GatewayPayload
    {
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
        } opcode;
        std::string_view data;

        i32              seq_num;
        std::string_view event_name;
    };

public:
    GLSbot(std::string_view bot_token);
};