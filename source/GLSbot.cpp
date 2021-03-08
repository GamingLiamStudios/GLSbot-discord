#include "GLSbot.h"
#include "util/types.h"

#include "discord-api/gateway.h"

#include <iostream>
#include <string_view>

#include <cpr/cpr.h>
#include <simdjson/simdjson.h>
#include <zlib-ng.h>

GLSbot::GLSbot(std::string_view bot_token)
{
    discordAPI::Gateway gateway(bot_token);
}