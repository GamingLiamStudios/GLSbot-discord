#include "GLSbot.h"

#include <iostream>
#include <string_view>
#include <json/json.hpp>

void GLSbot::start(std::string_view token)
{
    if (gateway.connect(token) < 0) std::__throw_runtime_error("Failed to connect to gateway");
    std::cout << "Successfully Connected to Gateway\n";

    while (gateway.connected())
    {
        int incoming = gateway.get_incoming();
        for (int i = 0; i < incoming; i++)
        {
            auto [event_name, event_data] = gateway.next_event();
            std::cout << event_name << "\n";
            // std::cout << event_data.dump(-1) << "\n";
        }
    }
}

void GLSbot::close()
{
    gateway.close();
}