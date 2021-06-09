#include <iostream>
#include <fstream>
#include <cstring>
#include <signal.h>
#include <filesystem>

#include "GLSbot.h"

#include <json/json.hpp>

GLSbot bot;

void sigint_callback(int signal)
{
    bot.close();
}

int main()
{
    ::srand((u32) time(NULL));

    std::string token;
    std::string user;
    {
        std::string json;
        if (!std::filesystem::exists("./config.json"))
        {
            std::cout << "config.json does not exist!\n";
            return -1;
        }

        auto auth = std::ifstream("./config.json", std::ios::in | std::ios::ate);
        json.resize(auth.tellg());
        auth.seekg(0, std::ios::beg);
        auth.read(json.data(), json.size());
        auth.close();

        auto parsed = nlohmann::json::parse(json);
        if (parsed.find("token") == parsed.end())
        {
            std::cout << "Bot token non-existant!\n";
            return -1;
        }
        token = parsed["token"].get<std::string>();

        if (parsed.find("owner_user") != parsed.end())
            user = parsed["owner_user"].get<std::string>();
    }

    signal(SIGINT, sigint_callback);
    bot.start(token, user);

    return 0;
}