#pragma once

#include "util/types.h"

namespace discordAPI
{
    struct Snowflake
    {
        u64 timestamp : 42;
        u8  iwID : 5;
        u8  ipID : 5;
        u16 inc : 12;
    };
}    // namespace discordAPI
