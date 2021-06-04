#pragma once

#include <string>

#include "discord-api/gateway.h"

class GLSbot
{
public:
    GLSbot() = default;

    void start(std::string_view token);

    void close();

private:
    std::string token;

    discordAPI::Gateway gateway;
};