#pragma once

#include <string>

#include "discord-api/gateway.h"

class GLSbot
{
public:
    GLSbot() = default;

    void start(std::string_view token, std::string_view owner_user);

    void close();
    void write_cache();

private:
    discordAPI::Gateway gateway;

    std::string              owner_id;
    std::vector<std::string> guilds;
};