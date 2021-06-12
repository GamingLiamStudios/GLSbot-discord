#pragma once

#include <string>

#include "discord/gateway.h"

class GLSbot
{
public:
    GLSbot() = default;

    void start(std::string_view token, std::string_view owner_user);

    void close();
    void write_cache();

private:
    discord::Gateway gateway;

    std::string              owner_id;
    std::vector<std::string> guilds;
};